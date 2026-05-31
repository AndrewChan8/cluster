from flask import Flask, jsonify, request
from flask_cors import CORS
import subprocess
import re

app = Flask(__name__)
CORS(app)

RPC_DIR = "/home/andrewsushi/cluster/rpc"

NODES = [
    {"name": "node1", "host": "node1", "port": "5000"},
    {"name": "node2", "host": "node2", "port": "5000"},
    {"name": "node3", "host": "node3", "port": "5000"},
]

dashboard_state = {
    "injected_fault": "None"
}


def parse_field(output, key, default="UNKNOWN"):
    match = re.search(rf"{key}=([^\s]+)", output)
    return match.group(1) if match else default


def get_last_entry(node):
    try:
        result = subprocess.run(
            ["./client", node["host"], node["port"], "log"],
            cwd=RPC_DIR,
            capture_output=True,
            text=True,
            timeout=2,
        )

        matches = re.findall(r'tx="([^"]*)"', result.stdout)

        if matches:
            return matches[-1]

        return "GENESIS / empty ledger"

    except Exception:
        return "UNKNOWN"

def poll_node(node):
    try:
        result = subprocess.run(
            ["./client", node["host"], node["port"], "dashboard"],
            cwd=RPC_DIR,
            capture_output=True,
            text=True,
            timeout=2,
        )

        output = result.stdout + result.stderr

        if result.returncode != 0:
            return offline_node(node, "OFFLINE")

        role = parse_field(output, "role")
        leader = parse_field(output, "leader")
        last_hash = parse_field(output, "last_hash")

        if role == "UNKNOWN" or leader == "UNKNOWN" or last_hash == "UNKNOWN":
            return offline_node(node, "OFFLINE")

        return {
            "name": parse_field(output, "node", node["name"]),
            "role": role,
            "leader": leader,
            "term": int(parse_field(output, "term", "0")),
            "mode": parse_field(output, "mode"),
            "adaptive": parse_field(output, "adaptive", "0") == "1",
            "status": "ONLINE",
            "ledgerSize": int(parse_field(output, "size", "0")),
            "lastHash": last_hash,
            "lastEntry": get_last_entry(node),
        }

    except subprocess.TimeoutExpired:
        return offline_node(node, "TIMEOUT")

    except Exception:
        return offline_node(node, "OFFLINE")

def offline_node(node, status):
    return {
        "name": node["name"],
        "role": "UNKNOWN",
        "leader": "UNKNOWN",
        "term": 0,
        "mode": "UNKNOWN",
        "adaptive": False,
        "status": status,
        "ledgerSize": 0,
        "lastHash": "UNKNOWN",
        "lastEntry": "UNKNOWN",
    }


@app.get("/api/status")
def status():
    nodes = [poll_node(node) for node in NODES]

    leader_node = next(
        (n for n in nodes if n["role"] == "LEADER"),
        None,
    )

    source = (
        leader_node
        if leader_node
        else next(
            (n for n in nodes if n["status"] == "ONLINE"),
            None,
        )
    )

    return jsonify({
        "leader": source["leader"] if source else "UNKNOWN",
        "mode": source["mode"] if source else "UNKNOWN",
        "adaptive": source["adaptive"] if source else False,
        "term": source["term"] if source else 0,
        "injectedFault": dashboard_state["injected_fault"],
        "nodes": nodes,
    })


@app.post("/api/adaptive/on")
def adaptive_on():
    leader = get_current_leader()

    if leader:
        run_client([
            leader,
            "5000",
            "adaptive",
            "on",
        ])

    return status()


@app.post("/api/adaptive/off")
def adaptive_off():
    leader = get_current_leader()

    if leader:
        run_client([
            leader,
            "5000",
            "adaptive",
            "off",
        ])

    return status()


@app.post("/api/failure/drop100")
def drop100():
    leader = get_current_leader()

    if leader:
        run_client([
            leader,
            "5000",
            "drop",
            "100",
        ])

    dashboard_state["injected_fault"] = "Drop 100%"

    return status()


@app.post("/api/failure/drop0")
def drop0():
    leader = get_current_leader()

    if leader:
        run_client([
            leader,
            "5000",
            "drop",
            "0",
        ])

    dashboard_state["injected_fault"] = "Drop 0%"

    return status()


@app.post("/api/failure/partition/node3")
def partition_node3():
    leader = get_current_leader()

    if leader:
        run_client([
            leader,
            "5000",
            "partition",
            "node3",
        ])

    dashboard_state["injected_fault"] = "Partition node3"

    return status()


@app.post("/api/failure/heal")
def heal():
    for node in NODES:
        run_client([
            node["host"],
            node["port"],
            "heal",
        ])

    dashboard_state["injected_fault"] = "None"

    return status()


@app.post("/api/append")
def append_tx():
    data = request.get_json(silent=True) or {}

    tx = data.get(
        "tx",
        "dash_key:dash_value",
    )

    leader = get_current_leader()

    if leader:
        run_client([
            leader,
            "5000",
            "append",
            tx,
        ])

    return status()


def get_current_leader():
    nodes = [poll_node(node) for node in NODES]

    leader_node = next(
        (n for n in nodes if n["role"] == "LEADER"),
        None,
    )

    return leader_node["name"] if leader_node else None


def run_client(args):
    try:
        subprocess.run(
            ["./client"] + args,
            cwd=RPC_DIR,
            capture_output=True,
            text=True,
            timeout=3,
        )

    except Exception:
        pass


if __name__ == "__main__":
    app.run(
        host="0.0.0.0",
        port=5050,
        debug=True,
    )