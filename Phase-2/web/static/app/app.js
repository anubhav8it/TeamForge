"use strict";
// Loads the TeamForge Qt for WebAssembly app for the signed-in account and keeps its data on
// the server.
//
// 1. GET /api/bootstrap: the account, the workspaces its role may open, and the data files
//    (shared data + this account's session.json).
// 2. window.TF_BOOT hands those to the app; src/web/WebBridge.cpp writes them into the app's data
//    folder before it starts, so the C++ code reads them exactly like the desktop app does.
// 3. The app calls window.TF_upload(name, text) whenever a data file's content changes; this
//    file uploads it with the version it was based on. If someone else changed that file first,
//    the server refuses (409) and the page asks for a reload instead of overwriting their work.
(function () {
  const $ = id => document.getElementById(id);
  const SESSION = "session.json";
  const state = {
    versions: {},   // file name -> server version this page's copy is based on
    canon: {},      // file name -> content key of the server copy (see canonical)
    uploads: {},    // file name -> { inflight, pending, retry, blocked }
    lastSaved: 0,
    offline: false,
    signedOut: false,
    noticeShown: false,
    instance: null,
  };

  // ---------------------------------------------------------------- page status
  function setSync() {
    const el = $("sync");
    const busy = pendingCount() > 0;
    if (state.offline) { el.textContent = "Offline – retrying…"; el.className = "sync error"; }
    else if (busy) { el.textContent = "Saving…"; el.className = "sync saving"; }
    else if (state.lastSaved) { el.textContent = "All changes saved"; el.className = "sync saved"; }
    else { el.textContent = ""; el.className = "sync"; }
  }

  function notice(text, kind) {
    $("notice-text").textContent = text;
    $("notice").className = "notice" + (kind === "error" ? " error" : "");
    state.noticeShown = true;
  }
  $("notice-reload").addEventListener("click", () => location.reload());

  function fail(text) {
    const overlay = $("loading");
    overlay.classList.remove("hidden");
    overlay.classList.add("error");
    $("loading-text").textContent = text;
  }

  // ---------------------------------------------------------------- content comparison
  // The app re-saves every data file whenever it saves, so a file can come back re-formatted
  // without any real change. Compare content, ignoring formatting, record order and the
  // top-level "note" (the server does the same).
  function sortDeep(value) {
    if (Array.isArray(value)) {
      return value.map(sortDeep).sort((a, b) => {
        const x = JSON.stringify(a), y = JSON.stringify(b);
        return x < y ? -1 : x > y ? 1 : 0;
      });
    }
    if (value && typeof value === "object") {
      const out = {};
      for (const key of Object.keys(value).sort()) out[key] = sortDeep(value[key]);
      return out;
    }
    return value;
  }
  function canonical(name, text) {
    if (name === ".sample-data") return String(text).trim();
    try {
      const value = JSON.parse(text);
      if (value && typeof value === "object" && !Array.isArray(value)) delete value.note;
      return JSON.stringify(sortDeep(value));
    } catch (e) {
      return String(text);
    }
  }

  // ---------------------------------------------------------------- uploads
  function pendingCount() {
    return Object.values(state.uploads).filter(u => u.inflight || u.pending !== null).length;
  }

  // Called by the app (WebBridge.cpp) with a data file's new content.
  window.TF_upload = function (name, text) {
    if (state.signedOut) return;
    if (name !== SESSION && state.canon[name] !== undefined && canonical(name, text) === state.canon[name])
      return; // re-saved, not changed
    const upload = state.uploads[name] || (state.uploads[name] = { inflight: false, pending: null, retry: 0, blocked: false });
    if (upload.blocked) return;
    upload.pending = String(text);
    pump(name);
  };

  async function pump(name) {
    const upload = state.uploads[name];
    if (upload.inflight || upload.pending === null) return;
    const text = upload.pending;
    upload.pending = null;
    upload.inflight = true;
    setSync();
    let retry = false;
    try {
      const res = await fetch("/api/files/" + encodeURIComponent(name), {
        method: "PUT",
        credentials: "same-origin",
        headers: { "Content-Type": "application/json", "X-TeamForge": "1" },
        body: JSON.stringify({ content: text, baseVersion: state.versions[name] || 0 }),
      });
      let data = {};
      try { data = await res.json(); } catch (e) { /* empty */ }
      state.offline = false;
      if (res.ok) {
        if (data.status === "saved" || data.status === "unchanged") {
          state.versions[name] = data.version;
          state.canon[name] = canonical(name, text);
          state.lastSaved = Date.now();
        }
        // "ignored": this account's role cannot change that file; the server copy stays.
      } else if (res.status === 409) {
        upload.blocked = true;
        notice("Someone else changed TeamForge data while you were working, so your last change was not saved. " +
               "Reload to get the latest data, then make the change again.", "error");
      } else if (res.status === 401) {
        state.signedOut = true;
        notice("You have been signed out, so your last change was not saved. Reload to sign in again.", "error");
      } else if (res.status >= 500) {
        retry = true;
      } else {
        notice("A change could not be saved: " + (data.error || "error " + res.status) + ".", "error");
      }
    } catch (e) {
      retry = true; // network problem (or the free server is waking up)
    }
    upload.inflight = false;
    if (retry) {
      state.offline = true;
      if (upload.pending === null) upload.pending = text;
      upload.retry += 1;
      setSync();
      setTimeout(() => pump(name), Math.min(30000, 2000 * upload.retry));
      return;
    }
    upload.retry = 0;
    setSync();
    pump(name);
  }

  // Tell the person when someone else saved changes, since the app loads data only at start.
  async function checkForUpdates() {
    if (state.noticeShown || state.signedOut || document.hidden || pendingCount() > 0) return;
    try {
      const res = await fetch("/api/versions", { credentials: "same-origin", cache: "no-store" });
      if (!res.ok || pendingCount() > 0) return;
      const data = await res.json();
      for (const [name, version] of Object.entries(data.versions || {})) {
        if ((state.versions[name] || 0) < version) {
          notice("Someone else updated TeamForge. Reload to see the latest data.", "info");
          return;
        }
      }
    } catch (e) { /* try again later */ }
  }

  // ---------------------------------------------------------------- account bar
  function showAccount(user) {
    const who = $("who");
    who.textContent = user.email;
    const chip = document.createElement("span");
    chip.className = "chip " + user.role;
    chip.textContent = user.role.toUpperCase();
    who.appendChild(chip);
    $("admin-link").classList.toggle("hidden", user.role !== "admin");
  }

  function flush() {
    try { if (state.instance && typeof state.instance.tfFlush === "function") state.instance.tfFlush(); }
    catch (e) { /* the app may have stopped */ }
  }

  $("signout").addEventListener("click", async () => {
    flush();
    const deadline = Date.now() + 5000;
    while (pendingCount() > 0 && Date.now() < deadline) await new Promise(r => setTimeout(r, 200));
    try {
      await fetch("/api/logout", {
        method: "POST", credentials: "same-origin",
        headers: { "Content-Type": "application/json", "X-TeamForge": "1" }, body: "{}",
      });
    } catch (e) { /* ignore */ }
    location.assign("/login");
  });

  window.addEventListener("beforeunload", event => {
    flush();
    if (pendingCount() > 0) { event.preventDefault(); event.returnValue = ""; }
  });

  // ---------------------------------------------------------------- start
  function findEntry() {
    if (typeof window.TeamForge_entry === "function") return window.TeamForge_entry;
    if (typeof window.teamforge_entry === "function") return window.teamforge_entry;
    for (const key of Object.getOwnPropertyNames(window)) {
      if (/_entry$/.test(key) && typeof window[key] === "function") return window[key];
    }
    return null;
  }

  async function start() {
    let boot;
    try {
      const res = await fetch("/api/bootstrap", { credentials: "same-origin", cache: "no-store" });
      if (res.status === 401) { location.assign("/login?reason=expired"); return; }
      if (!res.ok) throw new Error("status " + res.status);
      boot = await res.json();
    } catch (e) {
      fail("Could not reach the TeamForge server. Check your connection and reload the page.");
      return;
    }
    showAccount(boot.user);
    state.versions = Object.assign({}, boot.versions);
    for (const [name, text] of Object.entries(boot.files || {})) state.canon[name] = canonical(name, text);
    window.TF_BOOT = { allowedWorkspaces: boot.allowedWorkspaces, files: boot.files || {} };

    const entry = findEntry();
    if (typeof window.qtLoad !== "function" || !entry) {
      fail("The TeamForge app files (qtloader.js, TeamForge.js, TeamForge.wasm) are missing on the server. " +
           "Build the app for WebAssembly and copy them into web/static/app (see web/README-WEB.md).");
      return;
    }
    try {
      state.instance = await window.qtLoad({
        qt: {
          containerElements: [$("screen")],
          entryFunction: entry,
          onLoaded: () => { $("loading").classList.add("hidden"); setSync(); },
          onExit: exit => {
            fail("TeamForge stopped" + (exit && exit.text ? " (" + exit.text + ")" : "") + ". Reload the page to start again.");
          },
        },
      });
    } catch (e) {
      fail("TeamForge could not start in this browser (" + ((e && e.message) || e) + "). " +
           "Use a recent Chrome, Edge or Firefox with hardware acceleration turned on.");
      return;
    }
    setInterval(checkForUpdates, 20000);
  }

  start();
})();
