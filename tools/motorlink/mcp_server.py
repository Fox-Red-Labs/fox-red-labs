#!/usr/bin/env python3
"""motorlink MCP server: lets an AI assistant read a TI AM13 motor solution's live state over the MotorVision UART
protocol (see link.py). Read-only on purpose: it cannot start the motor or change a gain. Standard library only;
speaks MCP (JSON-RPC 2.0) over stdio.

Register it with an MCP client, for example in a project's .mcp.json:
  {"mcpServers": {"motorlink": {"command": "python3", "args": ["tools/motorlink/mcp_server.py"]}}}
"""
import json, pathlib, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import link as L  # noqa: E402

STATUS = ["motorState", "faultMtrNow", "ISRCount", "VdcBus_V", "speed_Hz", "speedRef_Hz",
          "Idq_in_A.value[0]", "Idq_in_A.value[1]", "I_A.value[0]", "I_A.value[1]", "I_A.value[2]"]

TOOLS = [
    {"name": "motor_status", "description": "Read the motor controller's state, decoded faults, bus voltage, speed and currents (about 1 s).",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "motor_read", "description": "Read up to 13 named variables from the firmware's MotorVision table, for a number of seconds.",
     "inputSchema": {"type": "object", "properties": {"names": {"type": "array", "items": {"type": "string"}},
                                                      "secs": {"type": "number", "default": 1.0}}, "required": ["names"]}},
    {"name": "motor_variables", "description": "List the variables the firmware exposes, with type and read/write permission.",
     "inputSchema": {"type": "object", "properties": {}}},
]


def call(name, args):
    if name == "motor_variables":
        return {k: {"type": v["type"], "permissions": v["permissions"]} for k, v in L.load_vars().items()}
    link = L.Link()
    try:
        if name == "motor_status":
            v = link.poll(STATUS, 1.0)
            st, f = v.get("motorVars_M1.outVars.motorState"), v.get("motorVars_M1.outVars.faultMtrNow")
            return {"state": L.STATES.get(st, st), "faults": L.decode_faults(f) if f is not None else None, "raw": v}
        if name == "motor_read":
            return link.poll(args["names"], float(args.get("secs", 1.0)))
    finally:
        link.close()
    raise ValueError(f"unknown tool {name}")


def reply(i, result=None, error=None):
    m = {"jsonrpc": "2.0", "id": i}
    m["error" if error else "result"] = error or result
    sys.stdout.write(json.dumps(m) + "\n"); sys.stdout.flush()


for line in sys.stdin:
    try:
        m = json.loads(line)
    except json.JSONDecodeError:
        continue
    meth, i = m.get("method"), m.get("id")
    if i is None:
        continue  # notifications
    if meth == "initialize":
        reply(i, {"protocolVersion": "2024-11-05", "capabilities": {"tools": {}},
                  "serverInfo": {"name": "motorlink", "version": "0.1"}})
    elif meth == "tools/list":
        reply(i, {"tools": TOOLS})
    elif meth == "tools/call":
        try:
            out = call(m["params"]["name"], m["params"].get("arguments", {}))
            reply(i, {"content": [{"type": "text", "text": json.dumps(out, indent=1)}]})
        except (SystemExit, Exception) as e:  # noqa: BLE001 - report every failure to the client as a tool error
            reply(i, {"content": [{"type": "text", "text": f"error: {e}"}], "isError": True})
    else:
        reply(i, error={"code": -32601, "message": f"method not found: {meth}"})
