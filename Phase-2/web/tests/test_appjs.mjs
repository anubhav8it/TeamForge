// Runs web/static/app/app.js in Node against the real server, with a stand-in for the Qt app
// that behaves like src/web/WebBridge.cpp (reads TF_BOOT, calls TF_upload on changes).
// Usage: start the server, create an account, then
//   BASE=http://localhost:8000 EMAIL=... PASSWORD=... node web/tests/test_appjs.mjs
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
const here = path.dirname(fileURLToPath(import.meta.url));
const BASE = process.env.BASE;
let cookie = "";
const results = [];
const check = (name, cond, detail = "") => { results.push(cond); console.log((cond ? "PASS " : "FAIL ") + name + (cond ? "" : "  -> " + JSON.stringify(detail))); };

const realFetch = globalThis.fetch;
async function rawFetch(path, opts = {}) {
  const headers = Object.assign({}, opts.headers || {}, cookie ? { Cookie: cookie } : {});
  const res = await realFetch(BASE + path, Object.assign({}, opts, { headers, redirect: "manual" }));
  const set = res.headers.getSetCookie();
  if (set.length) cookie = set[0].split(";")[0];
  return res;
}
// sign in as the participant created by the Python test run
const login = await rawFetch("/api/login", { method: "POST", headers: { "Content-Type": "application/json", "X-TeamForge": "1" },
  body: JSON.stringify({ email: process.env.EMAIL, password: process.env.PASSWORD }) });
check("node client signed in", login.status === 200, login.status);

// --- minimal DOM
const elements = {};
function el(id) {
  return elements[id] || (elements[id] = {
    id, textContent: "", className: "", children: [], attrs: {}, listeners: {},
    classList: { _s: new Set(), add(c) { this._s.add(c); }, remove(c) { this._s.delete(c); }, toggle(c, on) { on ? this._s.add(c) : this._s.delete(c); }, contains(c) { return this._s.has(c); } },
    addEventListener(t, f) { this.listeners[t] = f; }, appendChild(c) { this.children.push(c); }, setAttribute(k, v) { this.attrs[k] = v; },
  });
}
globalThis.window = globalThis;
globalThis.document = { hidden: false, getElementById: el, createElement: () => el("tmp" + Math.random()) };
let navigatedTo = null;
globalThis.location = { assign(u) { navigatedTo = u; }, reload() {} };
globalThis.addEventListener = () => {};
globalThis.fetch = (path, opts) => rawFetch(path, opts);

// --- stand-in for the Qt app (what WebBridge.cpp does)
const fsMem = {};
let loadedCalled = false;
globalThis.teamforge_entry = function () {};
globalThis.qtLoad = async function (config) {
  const boot = globalThis.TF_BOOT;
  for (const [n, t] of Object.entries(boot.files)) fsMem[n] = t;
  fsMem.__allowed = boot.allowedWorkspaces;
  config.qt.onLoaded();
  loadedCalled = true;
  return { tfFlush() {} };
};

const code = fs.readFileSync(path.join(here, "..", "static", "app", "app.js"), "utf8");
(0, eval)(code);
const sleep = ms => new Promise(r => setTimeout(r, ms));
await sleep(800);
check("app started (onLoaded called)", loadedCalled);
check("boot data handed to the app", Object.keys(fsMem).includes("students.json") && fsMem.__allowed.length > 0, Object.keys(fsMem));
check("account shown in the bar", el("who").textContent === process.env.EMAIL, el("who").textContent);

async function versions() { const r = await rawFetch("/api/versions"); return (await r.json()).versions; }
const v0 = await versions();

// 1. the app re-saves students.json with Qt formatting and no real change -> nothing uploaded
const reformatted = JSON.stringify(JSON.parse(fsMem["students.json"]), null, 4);
globalThis.TF_upload("students.json", reformatted);
await sleep(300);
check("re-saved unchanged file is not uploaded", (await versions())["students.json"] === v0["students.json"]);

// 2. a real profile change -> saved, version +1
const s = JSON.parse(fsMem["students.json"]); s.students[2].summary = "Changed from the browser test";
globalThis.TF_upload("students.json", JSON.stringify(s, null, 4));
await sleep(500);
const v1 = await versions();
check("real change uploaded (version +1)", v1["students.json"] === v0["students.json"] + 1, [v0["students.json"], v1["students.json"]]);
check("sync status shows saved", el("sync").textContent === "All changes saved", el("sync").textContent);

// 3. session.json
globalThis.TF_upload("session.json", JSON.stringify({ version: 1, lastWorkspace: "host", myProfileId: "tf-002", recentProjects: [] }));
await sleep(400);
const boot = await (await rawFetch("/api/bootstrap")).json();
check("session.json saved for this account", JSON.parse(boot.files["session.json"]).myProfileId === "tf-002");

// 4. someone else saves students.json first -> this page's next change is refused, notice shown
const other = JSON.parse(boot.files["students.json"]); other.students[3].summary = "Someone else";
const r = await rawFetch("/api/files/students.json", { method: "PUT", headers: { "Content-Type": "application/json", "X-TeamForge": "1" },
  body: JSON.stringify({ content: JSON.stringify(other), baseVersion: boot.versions["students.json"] }) });
check("other client saved first", r.status === 200);
s.students[4].summary = "My second change";
globalThis.TF_upload("students.json", JSON.stringify(s, null, 4));
await sleep(500);
const after = await (await rawFetch("/api/bootstrap")).json();
check("stale change did not overwrite the other person's", JSON.parse(after.files["students.json"]).students[3].summary === "Someone else");
check("conflict notice shown", el("notice").className === "notice error" && /Reload/.test(el("notice-text").textContent), el("notice-text").textContent);

console.log(`\n${results.filter(Boolean).length}/${results.length} passed`);
process.exit(0);
