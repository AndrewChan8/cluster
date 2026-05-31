import { useEffect, useState } from "react";
import { Activity, Database, Shield, WifiOff, Clock } from "lucide-react";

const API = "http://node1:5050";

export default function App() {
  const [cluster, setCluster] = useState(null);
  const [lastUpdated, setLastUpdated] = useState("");
  const [tx, setTx] = useState("account1:deposit100");
  const [message, setMessage] = useState("");

  async function fetchStatus() {
    const res = await fetch(`${API}/api/status`);
    const data = await res.json();
    setCluster(data);
    setLastUpdated(new Date().toLocaleTimeString());
  }

  async function post(path, body = null, label = "Command sent") {
    try {
      await fetch(`${API}${path}`, {
        method: "POST",
        headers: body ? { "Content-Type": "application/json" } : {},
        body: body ? JSON.stringify(body) : null,
      });
      setMessage(label);
      fetchStatus();
    } catch {
      setMessage("Command failed");
    }
  }

  useEffect(() => {
    fetchStatus();
    const id = setInterval(fetchStatus, 1000);
    return () => clearInterval(id);
  }, []);

  if (!cluster) {
    return <div className="min-h-screen bg-slate-950 text-white p-8">Loading cluster...</div>;
  }

  const onlineNodes = cluster.nodes.filter(
    (n) => n.status === "ONLINE" && n.lastHash !== "UNKNOWN"
  );
  const majorityHash = mostCommon(onlineNodes.map((n) => n.lastHash));

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 p-8">
      <div className="mb-8 flex justify-between items-start">
        <div>
          <p className="text-sm text-cyan-400 font-medium">CS630 Distributed Systems</p>
          <h1 className="text-4xl font-bold">Adaptive Replicated Ledger</h1>
          <p className="text-slate-400 mt-2">
            Live dashboard for leader election, replication, failures, and adaptive consistency.
          </p>
        </div>
        <p className="text-sm text-slate-500">Last updated: {lastUpdated}</p>
      </div>

      <div className="grid grid-cols-5 gap-4 mb-8">
        <Metric title="Leader" value={cluster.leader} icon={<Shield />} />
        <Metric title="Mode" value={cluster.mode} icon={<Activity />} />
        <Metric title="Adaptive" value={cluster.adaptive ? "ON" : "OFF"} icon={<Database />} />
        <Metric title="Term" value={cluster.term ?? 0} icon={<Clock />} />
        <Metric title="Injected Fault" value={cluster.injectedFault ?? "None"} icon={<WifiOff />} />
      </div>

      <div className="grid grid-cols-3 gap-6 mb-8">
        {cluster.nodes.map((node) => {
          const diverged =
            node.status === "ONLINE" &&
            node.lastHash !== "UNKNOWN" &&
            majorityHash &&
            node.lastHash !== majorityHash;

          return (
            <div
              key={node.name}
              className={`rounded-2xl border p-6 shadow-xl ${
                diverged ? "border-yellow-500/60 bg-yellow-950/20" : "border-slate-800 bg-slate-900"
              }`}
            >
              <div className="flex justify-between items-center">
                <h2 className="text-2xl font-semibold">{node.name}</h2>
                <span className={`rounded-full px-3 py-1 text-sm ${statusClass(node.status)}`}>
                  {node.status}
                </span>
              </div>

              <div className="mt-5 space-y-2 text-slate-300">
                <p>Role: <span className={roleClass(node.role)}>{node.role}</span></p>
                <p>Term: <span className="text-white">{node.term}</span></p>
                <p>Known Leader: <span className="text-white">{node.leader}</span></p>
                <p>Mode: <span className="text-white">{node.mode}</span></p>
                <p>Adaptive: <span className="text-white">{node.adaptive ? "ON" : "OFF"}</span></p>
                <p>Ledger Size: <span className="text-white">{node.ledgerSize}</span></p>

                <div>
                  <p>Last Hash:</p>
                  <div className="mt-1 overflow-x-auto rounded-lg border border-slate-700 bg-slate-950/60 px-3 py-2">
                    <p className="font-mono text-xs text-slate-300 whitespace-nowrap">
                      {node.lastHash}
                    </p>
                  </div>
                </div>

                <div>
                  <p>Last Ledger Entry:</p>
                  <div className="mt-1 rounded-lg border border-slate-700 bg-slate-950/60 px-3 py-2">
                    <p className="font-mono text-xs text-slate-300 break-all">
                      {node.lastEntry ?? "UNKNOWN"}
                    </p>
                  </div>
                </div>

                {diverged && <p className="text-yellow-400 font-semibold">Diverged from majority hash</p>}
              </div>
            </div>
          );
        })}
      </div>

      <div className="grid grid-cols-3 gap-6">
        <div className="col-span-2 rounded-2xl border border-slate-800 bg-slate-900 p-6">
          <h2 className="text-2xl font-semibold mb-4">Controls</h2>

          <div className="mb-5 flex gap-3">
            <input
              value={tx}
              onChange={(e) => setTx(e.target.value)}
              className="w-full rounded-xl border border-slate-700 bg-slate-950 px-4 py-2 text-slate-100 outline-none focus:border-cyan-400"
              placeholder="account1:deposit100"
            />
            <Button label="Append Tx" onClick={() => post("/api/append", { tx }, "Append submitted")} />
          </div>

          <div className="flex flex-wrap gap-3">
            <Button label="Adaptive On" onClick={() => post("/api/adaptive/on", null, "Adaptive enabled")} />
            <Button label="Adaptive Off" onClick={() => post("/api/adaptive/off", null, "Adaptive disabled")} />
            <Button label="Drop 100%" onClick={() => post("/api/failure/drop100", null, "Drop 100% injected")} />
            <Button label="Drop 0%" onClick={() => post("/api/failure/drop0", null, "Drop cleared")} />
            <Button label="Partition node3" onClick={() => post("/api/failure/partition/node3", null, "Partition node3 requested")} />
            <Button label="Heal" onClick={() => post("/api/failure/heal", null, "Heal requested")} />
          </div>

          {message && <p className="mt-4 text-sm text-cyan-300">{message}</p>}
        </div>

        <div className="rounded-2xl border border-slate-800 bg-slate-900 p-6">
          <h2 className="text-2xl font-semibold mb-4">Demo Flow</h2>
          <ol className="space-y-3 text-sm text-slate-300">
            <li><span className="text-cyan-400 font-bold">1.</span> Show healthy replicated cluster.</li>
            <li><span className="text-cyan-400 font-bold">2.</span> Append a transaction and watch all ledgers update.</li>
            <li><span className="text-cyan-400 font-bold">3.</span> Kill the leader and observe failover.</li>
            <li><span className="text-cyan-400 font-bold">4.</span> Enable adaptive consistency.</li>
            <li><span className="text-cyan-400 font-bold">5.</span> Inject failure and observe mode changes.</li>
            <li><span className="text-cyan-400 font-bold">6.</span> Heal and verify convergence.</li>
          </ol>
        </div>
      </div>
    </div>
  );
}

function Metric({ title, value, icon }) {
  return (
    <div className="rounded-2xl border border-slate-800 bg-slate-900 p-5">
      <div className="text-cyan-400 mb-3">{icon}</div>
      <p className="text-sm text-slate-400">{title}</p>
      <p className="text-2xl font-bold">{value}</p>
    </div>
  );
}

function Button({ label, onClick }) {
  return (
    <button
      onClick={onClick}
      className="rounded-xl bg-cyan-500 px-4 py-2 font-semibold text-slate-950 hover:bg-cyan-400"
    >
      {label}
    </button>
  );
}

function statusClass(status) {
  if (status === "ONLINE") return "bg-emerald-500/10 text-emerald-400";
  if (status === "TIMEOUT") return "bg-yellow-500/10 text-yellow-400";
  return "bg-red-500/10 text-red-400";
}

function roleClass(role) {
  if (role === "LEADER") return "text-yellow-300 font-bold";
  if (role === "CANDIDATE") return "text-orange-300 font-bold";
  if (role === "FOLLOWER") return "text-cyan-300 font-bold";
  return "text-slate-400";
}

function mostCommon(values) {
  if (values.length === 0) return null;

  const counts = {};
  for (const v of values) {
    counts[v] = (counts[v] || 0) + 1;
  }

  return Object.entries(counts).sort((a, b) => b[1] - a[1])[0][0];
}