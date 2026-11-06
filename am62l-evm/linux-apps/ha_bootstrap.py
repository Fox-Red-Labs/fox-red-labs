#!/usr/bin/env python3
"""Home Assistant Container: finish onboarding, add the MQTT integration, make one MQTT sensor appear.
Standard library only; runs on the Mac or on the board.
  ha_bootstrap.py HA_URL BROKER_HOST BROKER_PORT PUBLISH_HOST PUBLISH_PORT
Steps: wait for the web page, create the first user (onboarding API), get a token, finish onboarding,
add the MQTT config entry, publish a discovery message, read the sensor state back through /api/states.
Prints one 'STEP name seconds' line per step and 'RESULT ok sensor=<state>' on success."""
import json, sys, time, urllib.request, urllib.parse, urllib.error, os, secrets
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mqtt_min

ha, bhost, bport, phost, pport = sys.argv[1].rstrip('/'), sys.argv[2], int(sys.argv[3]), sys.argv[4], int(sys.argv[5])
CID = ha + '/'
# The bench user's password: HA_BENCH_PASSWORD in the environment, or a random one printed once (never in the repo).
PW = os.environ.get('HA_BENCH_PASSWORD') or secrets.token_urlsafe(12)
if not os.environ.get('HA_BENCH_PASSWORD'):
    print('bench user frl, generated password:', PW, '(set HA_BENCH_PASSWORD to reuse it)', file=sys.stderr)
T0 = time.time()


def step(n):
    print(f'STEP {n} {time.time()-T0:.1f}', flush=True)


def call(method, path, body=None, token=None, form=False, timeout=60):
    data = None
    h = {}
    if body is not None:
        if form:
            data = urllib.parse.urlencode(body).encode(); h['Content-Type'] = 'application/x-www-form-urlencoded'
        else:
            data = json.dumps(body).encode(); h['Content-Type'] = 'application/json'
    if token:
        h['Authorization'] = 'Bearer ' + token
    req = urllib.request.Request(ha + path, data=data, headers=h, method=method)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            raw = r.read()
            return r.status, (json.loads(raw) if raw and raw[:1] in b'{[' else raw)
    except urllib.error.HTTPError as e:
        return e.code, e.read()[:300]


t = time.time()
while True:
    try:
        c, _ = call('GET', '/', timeout=10)
        if c == 200:
            break
    except Exception:
        pass
    if time.time() - t > 1500:
        sys.exit('RESULT fail web page never loaded')
    time.sleep(5)
step('web-page-200')
c, steps = call('GET', '/api/onboarding')
print('onboarding', c, steps)
tok = None
if any(s['step'] == 'user' and not s['done'] for s in steps):
    c, r = call('POST', '/api/onboarding/users', {'client_id': CID, 'name': 'FRL', 'username': 'frl', 'password': PW, 'language': 'en'})
    code = r['auth_code']
    c, r = call('POST', '/auth/token', {'grant_type': 'authorization_code', 'code': code, 'client_id': CID}, form=True)
    tok = r['access_token']
    step('user-created')
else:
    c, r = call('POST', '/auth/login_flow', {'client_id': CID, 'handler': ['homeassistant', None], 'redirect_uri': CID})
    fid = r['flow_id']
    c, r = call('POST', f'/auth/login_flow/{fid}', {'username': 'frl', 'password': PW, 'client_id': CID})
    c, r = call('POST', '/auth/token', {'grant_type': 'authorization_code', 'code': r['result'], 'client_id': CID}, form=True)
    tok = r['access_token']
for s in ('core_config', 'analytics'):
    call('POST', f'/api/onboarding/{s}', {}, token=tok)
call('POST', '/api/onboarding/integration', {'client_id': CID, 'redirect_uri': CID}, token=tok)
step('onboarding-finished')
c, r = call('POST', '/api/config/config_entries/flow', {'handler': 'mqtt', 'show_advanced_options': False}, token=tok)
print('flow', c, str(r)[:200])
fid = r['flow_id']
if r.get('step_id') == 'broker':
    # HA 2026 requires the collapsed 'other_settings' section with its two required keys.
    c, r = call('POST', f'/api/config/config_entries/flow/{fid}', {'broker': bhost, 'port': bport, 'protocol': '3.1.1', 'other_settings': {'set_client_cert': False, 'set_ca_cert': 'off', 'transport': 'tcp'}}, token=tok)
    print('broker', c, str(r)[:300])
    if c != 200 or not isinstance(r, dict):
        sys.exit(f'RESULT fail MQTT broker step: {c} {r}')
if r.get('type') != 'create_entry' and r.get('step_id') == 'hassio_confirm':
    c, r = call('POST', f'/api/config/config_entries/flow/{fid}', {}, token=tok)
step('mqtt-entry:' + str(r.get('type')))
time.sleep(5)
cfg = {'name': 'FRL Test Sensor', 'unique_id': 'frl_test_1', 'state_topic': 'frl/test/temp', 'unit_of_measurement': 'C',
       'device': {'identifiers': ['frl_dev_1'], 'name': 'FRL bench'}}
mqtt_min.publish(phost, pport, 'homeassistant/sensor/frl_test_1/config', json.dumps(cfg), retain=True)
time.sleep(4)
mqtt_min.publish(phost, pport, 'frl/test/temp', '21.5', retain=True)
step('published')
t = time.time()
state = None
while time.time() - t < 60:
    c, r = call('GET', '/api/states/sensor.frl_bench_frl_test_sensor', token=tok)
    if c != 200:
        c, r = call('GET', '/api/states/sensor.frl_test_sensor', token=tok)
    if c == 200 and isinstance(r, dict) and r.get('state') not in (None, 'unknown'):
        state = r['state']; break
    time.sleep(2)
step('sensor-state')
if state is None:
    c, r = call('GET', '/api/states', token=tok)
    print('states', [x['entity_id'] for x in r][:20] if isinstance(r, list) else r)
    sys.exit('RESULT fail no sensor state')
print(f'RESULT ok sensor={state}')
