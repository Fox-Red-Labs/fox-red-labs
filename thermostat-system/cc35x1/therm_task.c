/* ---- thermostat additions (thermostat-system/cc35x1/build.sh) ---- */
typedef struct { uint32_t seq; int presence; int setpoint; int temp; } ThermState_t;
static ThermState_t gTherm;
static volatile int gThermPending;
static volatile uint32_t gThermLines, gThermBad;

/* strict decimal; returns 0 on success */
static int thermNum(const char *s, long *v)
{
    char *e;
    if (*s == 0) return -1;
    *v = strtol(s, &e, 10);
    return (*e == 0) ? 0 : -1;
}

/* "T,<seq>,<presence>,<zones>,<setpoint>,<temp>" ; one bad field drops the whole line */
static void thermParse(char *l)
{
    char *f[6];
    int n = 0;
    char *p = l;
    long seq, pres, sp, tmp;
    if (l[0] != 'T' || l[1] != ',') return;
    f[n++] = l; /* field 0 is "T" */
    for (p = l; *p && n < 6; p++) { if (*p == ',') { *p = 0; f[n++] = p + 1; } }
    if (n != 6 || strchr(f[5], ',')) { gThermBad++; return; }
    if (thermNum(f[1], &seq) || thermNum(f[2], &pres) || thermNum(f[4], &sp) || thermNum(f[5], &tmp)) { gThermBad++; return; }
    if ((pres != 0 && pres != 1) || seq < 0 || sp < -400 || sp > 1000 || tmp < -400 || tmp > 1000) { gThermBad++; return; }
    taskENTER_CRITICAL();
    gTherm.seq = (uint32_t)seq; gTherm.presence = (int)pres; gTherm.setpoint = (int)sp; gTherm.temp = (int)tmp;
    taskEXIT_CRITICAL();
    gThermLines++;
    if (!gThermPending)
    {
        struct msgQueue q;
        q.event = APP_MQTT_THERM;
        gThermPending = 1;
        if (osi_MsgQWrite(&appQueue, &q, OSI_NO_WAIT, OSI_FLAG_NOT_FROM_INTR) < 0) gThermPending = 0;
    }
}

/* UART0 (GPIO18 RX / GPIO17 TX), 115200 8N1, blocking partial reads, line buffer */
static void thermUartTask(void *arg)
{
    UART2_Params p;
    UART2_Handle u;
    uint8_t buf[32];
    char line[80];
    int len = 0;
    size_t got;
    (void)arg;
    UART2_Params_init(&p);
    p.baudRate = 115200;
    p.readReturnMode = UART2_ReadReturnMode_PARTIAL;
    u = UART2_open(CONFIG_UART2_1, &p);
    if (!u) { UART_PRINT("thermostat UART0 open failed\r\n"); vTaskDelete(NULL); return; }
    for (;;)
    {
        got = 0;
        if (UART2_read(u, buf, sizeof buf, &got) != UART2_STATUS_SUCCESS) { vTaskDelay(1); continue; }
        for (size_t i = 0; i < got; i++)
        {
            char c = (char)buf[i];
            if (c == '\n' || c == '\r')
            {
                if (len > 0) { line[len] = 0; thermParse(line); }
                len = 0;
            }
            else if (len < (int)sizeof line - 1) line[len++] = c;
            else len = 0; /* overlong line: drop it */
        }
    }
}

static void thermFmt10(char *out, int v) /* tenths of a degree to "21.5" */
{
    int a = v < 0 ? -v : v;
    sprintf(out, "%s%d.%d", v < 0 ? "-" : "", a / 10, a % 10);
}

/* presence, setpoint, temp, then seq last; retained, QoS 0 */
static void thermPublish(MQTTClient_Handle h)
{
    ThermState_t s;
    char b[16];
    gThermPending = 0;
    taskENTER_CRITICAL();
    s = gTherm;
    taskEXIT_CRITICAL();
    MQTT_IF_publish(h, "thermostat/presence", s.presence ? "ON" : "OFF", s.presence ? 2 : 3, MQTT_QOS_0 | 0x100);
    osi_uSleep(3000);
    thermFmt10(b, s.setpoint);
    MQTT_IF_publish(h, "thermostat/setpoint", b, strlen(b), MQTT_QOS_0 | 0x100);
    osi_uSleep(3000);
    thermFmt10(b, s.temp);
    MQTT_IF_publish(h, "thermostat/temp", b, strlen(b), MQTT_QOS_0 | 0x100);
    osi_uSleep(3000);
    sprintf(b, "%lu", (unsigned long)s.seq);
    MQTT_IF_publish(h, "thermostat/seq", b, strlen(b), MQTT_QOS_0 | 0x100);
    UART_PRINT("therm seq=%lu lines=%lu bad=%lu\r\n", (unsigned long)s.seq, (unsigned long)gThermLines, (unsigned long)gThermBad);
}
