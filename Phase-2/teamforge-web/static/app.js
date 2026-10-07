"use strict";
/* TeamForge web frontend. Plain JS, no build step.
   Every value from the server goes through esc() before it touches innerHTML. */

const $ = (s, el = document) => el.querySelector(s);
const $$ = (s, el = document) => [...el.querySelectorAll(s)];
const view = $("#view");
let ME = { user: null, profile: null };

const LEVELS = ["", "Beginner", "Basic", "Intermediate", "Advanced", "Expert"];
const STATUS = { interested: "Interested", under_review: "Under review", accepted: "Accepted", declined: "Declined" };
const PROJECT_TYPES = ["Hackathon", "PBL / Course Project", "AI/ML Project", "Web / Product Project",
  "Research Project", "Technology Competition", "Other"];

// ------------------------------------------------------------ utilities
function esc(v) {
  return String(v ?? "").replace(/[&<>"']/g, c =>
    ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
}

async function api(path, { method = "GET", body } = {}) {
  const res = await fetch(path, {
    method, credentials: "same-origin",
    headers: body !== undefined ? { "Content-Type": "application/json" } : {},
    body: body !== undefined ? JSON.stringify(body) : undefined,
  });
  let data = {};
  try { data = await res.json(); } catch { /* empty body */ }
  if (!res.ok) {
    const err = new Error(data.error || `Request failed (${res.status}).`);
    err.status = res.status;
    if (res.status === 401) { ME = { user: null, profile: null }; go("#/login"); }
    throw err;
  }
  return data;
}

let toastTimer;
function toast(msg, bad = false) {
  const t = $("#toast");
  t.textContent = msg;
  t.className = "toast show" + (bad ? " bad" : "");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => (t.className = "toast"), 2800);
}

/** Run an async action; disables the button meanwhile (stops double clicks). */
async function act(btn, fn) {
  if (btn) btn.disabled = true;
  try { return await fn(); }
  catch (e) { toast(e.message, true); }
  finally { if (btn) btn.disabled = false; }
}

function openModal(title, html) {
  $("#modal-title").textContent = title;
  $("#modal-body").innerHTML = html;
  $("#modal").hidden = false;
  $("#modal [data-close]").focus();
  return $("#modal-body");
}
function closeModal() { $("#modal").hidden = true; }
$("#modal").addEventListener("click", e => {
  if (e.target.id === "modal" || e.target.closest("[data-close]")) closeModal();
});
document.addEventListener("keydown", e => { if (e.key === "Escape") closeModal(); });

const home = () => (isHost() ? "#/host" : ME.profile ? "#/opportunities" : "#/profile");
const go = hash => { if (location.hash !== hash) location.hash = hash; else render(); };
const debounce = (fn, ms = 200) => { let t; return (...a) => { clearTimeout(t); t = setTimeout(() => fn(...a), ms); }; };

// ------------------------------------------------------------ small components
const chip = (name, level, cls = "") =>
  `<span class="chip ${cls}" title="${esc(name)}">${esc(name)}${level ? ` <i>${level}</i>` : ""}</span>`;

const chips = skills => `<div class="chips">${skills.map(s => chip(s.name, s.level)).join("")}</div>`;

function matchBadge(pct) {
  return `<span class="match" aria-label="${pct}% match"><span class="bar"><span style="width:${pct}%"></span></span>${pct}%</span>`;
}

/** The skill ladder: outlined cells = required level, filled = level held. */
function ladder(rows) {
  return `<div class="ladder">${rows.map(r => {
    const cells = [1, 2, 3, 4, 5].map(i =>
      `<span class="cell${i <= r.required ? " need" : ""}${i <= r.have ? " have" : ""}"></span>`).join("");
    const tag = r.status === "met" ? "Met" : r.status === "partial" ? `${r.have} of ${r.required}` : "Missing";
    return `<div class="rung ${r.status}"><span class="name" title="${esc(r.skill)}">${esc(r.skill)}</span>
      <span class="cells" aria-hidden="true">${cells}</span><span class="tag">${tag}</span></div>`;
  }).join("")}</div>
  <p class="hint">Outlined squares show the level needed; filled squares show the level held.</p>`;
}

function showWhy(name, pct, why) {
  const met = why.filter(r => r.status === "met").length;
  openModal(`Why this match? · ${name}`, `
    <p><strong>${pct}% match.</strong> Meets ${met} of ${why.length} required skills.
    Each skill counts up to its required level; going above it adds nothing extra.</p>
    ${ladder(why)}`);
}

function insights(why) {
  const gaps = why.filter(r => r.status !== "met");
  if (!gaps.length) return "Meets every required skill.";
  const top = gaps.slice(0, 3).map(r => r.skill.toUpperCase()).join(", ");
  return `Biggest gaps: ${top}.`;
}

/** Autocomplete over the skill catalogue. onPick(name) gets the chosen text. */
function autocomplete(input, onPick) {
  const wrap = input.parentElement;
  wrap.classList.add("ac");
  const ul = document.createElement("ul");
  ul.setAttribute("role", "listbox");
  ul.hidden = true;
  wrap.appendChild(ul);
  input.setAttribute("autocomplete", "off");
  let items = [], idx = -1;

  const draw = () => {
    ul.innerHTML = items.map((it, i) =>
      `<li role="option" aria-selected="${i === idx}" data-i="${i}">${esc(it.name.toUpperCase())}
       <span class="muted small">${esc(it.category)}</span></li>`).join("");
    ul.hidden = !items.length;
  };
  const search = debounce(async () => {
    const q = input.value.trim();
    if (!q) { items = []; draw(); return; }
    try { items = (await api(`/api/skills?q=${encodeURIComponent(q)}`)).items; } catch { items = []; }
    idx = -1; draw();
  });
  const pick = name => { onPick(name); input.value = ""; items = []; draw(); input.focus(); };

  input.addEventListener("input", search);
  input.addEventListener("keydown", e => {
    if (e.key === "ArrowDown" && items.length) { idx = (idx + 1) % items.length; draw(); e.preventDefault(); }
    else if (e.key === "ArrowUp" && items.length) { idx = (idx - 1 + items.length) % items.length; draw(); e.preventDefault(); }
    else if (e.key === "Enter") {
      e.preventDefault();
      const v = idx >= 0 ? items[idx].name : input.value.trim();
      if (v) pick(v);   // typing a brand-new skill and pressing Enter is allowed
    }
  });
  ul.addEventListener("mousedown", e => {
    const li = e.target.closest("li");
    if (li) { e.preventDefault(); pick(items[+li.dataset.i].name); }
  });
  input.addEventListener("blur", () => setTimeout(() => { items = []; draw(); }, 120));
}

/** Editable list of skills. withLevel=false gives plain names (learning interests). */
function skillEditor(el, initial, { withLevel = true, levelLabel = "Level", onChange = () => {} } = {}) {
  let list = initial.map(s => (typeof s === "string" ? { name: s } : { ...s }));
  el.innerHTML = `<div class="rows"></div>
    <div class="field" style="margin:0"><div><input placeholder="Type a skill, then pick or press Enter" aria-label="Add skill"></div></div>`;
  const rows = $(".rows", el);
  const draw = () => {
    rows.innerHTML = list.map((s, i) => `<div class="skill-row" style="${withLevel ? "" : "grid-template-columns:1fr auto"}">
      <span class="name">${esc(s.name)}</span>
      ${withLevel ? `<select data-i="${i}" aria-label="${levelLabel} for ${esc(s.name)}">
        ${[1, 2, 3, 4, 5].map(l => `<option value="${l}" ${l === s.level ? "selected" : ""}>${l} · ${LEVELS[l]}</option>`).join("")}
      </select>` : ""}
      <button type="button" class="icon-btn" data-rm="${i}" aria-label="Remove ${esc(s.name)}">✕</button></div>`).join("");
    onChange(list);
  };
  rows.addEventListener("change", e => { if (e.target.dataset.i) list[+e.target.dataset.i].level = +e.target.value; });
  rows.addEventListener("click", e => { const b = e.target.closest("[data-rm]"); if (b) { list.splice(+b.dataset.rm, 1); draw(); } });
  autocomplete($("input", el), name => {
    const n = name.trim().replace(/\s+/g, " ").toLowerCase();
    if (n.length > 40) { toast("Skill names can be at most 40 characters.", true); return; }
    if (list.some(s => s.name === n)) { toast(`${n.toUpperCase()} is already added.`, true); return; }
    list.push(withLevel ? { name: n, level: 3 } : { name: n });
    draw();
  });
  draw();
  return { get: () => (withLevel ? list.map(s => ({ name: s.name, level: s.level })) : list.map(s => s.name)) };
}

// ------------------------------------------------------------ navigation
const isHost = () => ME.user && (ME.user.role === "host" || ME.user.role === "admin");
const isAdmin = () => ME.user && ME.user.role === "admin";

function drawNav(path) {
  const nav = $("#nav");
  if (!ME.user) { nav.innerHTML = ""; $("#app").style.gridTemplateColumns = "1fr"; nav.hidden = true; return; }
  nav.hidden = false; $("#app").style.gridTemplateColumns = "";
  const link = (href, label) => `<a href="${href}" ${path.startsWith(href.slice(1)) ? 'aria-current="page"' : ""}>${label}</a>`;
  nav.innerHTML = `<div class="brand">Team<span>Forge</span></div>
    <div class="nav-group">Participant</div>
    ${link("#/profile", "My profile")}${link("#/opportunities", "Opportunities")}
    ${isHost() ? `<div class="nav-group">Host</div>${link("#/host", "Projects")}${link("#/people", "Participants")}` : ""}
    ${isAdmin() ? `<div class="nav-group">Admin</div>${link("#/admin", "Overview")}${link("#/skills", "Skills")}` : ""}
    <a href="#/logout">Sign out</a>
    <div class="who">${esc(ME.user.email)}<br>${esc(ME.user.role)}</div>`;
}

const routes = [
  [/^\/login$/, viewAuth, null],
  [/^\/logout$/, async () => { await api("/api/auth/logout", { method: "POST", body: {} }); ME = { user: null }; go("#/login"); }, "any"],
  [/^\/profile$/, viewProfile, "any"],
  [/^\/opportunities$/, viewOpportunities, "any"],
  [/^\/host$/, viewHost, "host"],
  [/^\/host\/new$/, () => viewProjectForm(null), "host"],
  [/^\/host\/edit\/([\w-]+)$/, id => viewProjectForm(id), "host"],
  [/^\/host\/team\/([\w-]+)$/, viewTeamBuilder, "host"],
  [/^\/host\/applicants\/([\w-]+)$/, viewApplicants, "host"],
  [/^\/people$/, viewPeople, "host"],
  [/^\/admin$/, viewAdmin, "admin"],
  [/^\/skills$/, viewSkills, "admin"],
];

async function render() {
  const path = location.hash.slice(1) || "/";
  closeModal();
  if (!ME.user && path !== "/login") return go("#/login");
  if (ME.user && (path === "/" || path === "/login")) return go(home());
  const hit = routes.find(([re]) => re.test(path));
  if (!hit) return go("#/profile");
  const [re, fn, role] = hit;
  if ((role === "host" && !isHost()) || (role === "admin" && !isAdmin())) {
    view.innerHTML = `<div class="empty">You don't have access to this page.</div>`;
    return drawNav(path);
  }
  drawNav(path);
  view.onclick = view.onchange = null;   // drop the previous page's handlers
  view.innerHTML = `<p class="muted">Loading…</p>`;
  try { await fn(...path.match(re).slice(1)); }
  catch (e) { view.innerHTML = `<div class="empty">${esc(e.message)}</div>`; }
  view.focus();
}
window.addEventListener("hashchange", render);

// ------------------------------------------------------------ auth
function viewAuth() {
  view.innerHTML = `<section class="entry">
    <div class="wordmark">Team<span>Forge</span></div>
    <p class="muted">Find project teammates by skill. Hosts see who fits; participants see where they fit.</p>
    <div class="tabs" role="tablist">
      <button role="tab" aria-selected="true" data-tab="login">Sign in</button>
      <button role="tab" aria-selected="false" data-tab="register">Create account</button>
    </div>
    <form id="auth" class="panel" novalidate>
      <div class="field"><label for="email">Email</label><input id="email" type="email" autocomplete="email" required></div>
      <div class="field"><label for="pw">Password</label><input id="pw" type="password" autocomplete="current-password" minlength="8" required>
        <div class="hint" id="pw-hint" hidden>At least 8 characters.</div></div>
      <p class="error" id="auth-err" hidden></p>
      <button class="btn primary" id="auth-btn" type="submit">Sign in</button>
    </form>
    <p class="hint">New accounts start as participants. An admin can make you a host.</p></section>`;
  let mode = "login";
  $(".tabs").addEventListener("click", e => {
    const b = e.target.closest("[data-tab]"); if (!b) return;
    mode = b.dataset.tab;
    $$(".tabs button").forEach(x => x.setAttribute("aria-selected", x === b));
    $("#auth-btn").textContent = mode === "login" ? "Sign in" : "Create account";
    $("#pw-hint").hidden = mode === "login";
    $("#pw").autocomplete = mode === "login" ? "current-password" : "new-password";
  });
  $("#auth").addEventListener("submit", e => {
    e.preventDefault();
    act($("#auth-btn"), async () => {
      try {
        ME = await api(`/api/auth/${mode}`, { method: "POST", body: { email: $("#email").value, password: $("#pw").value } });
        go(home());
      } catch (err) { const p = $("#auth-err"); p.textContent = err.message; p.hidden = false; }
    });
  });
}

// ------------------------------------------------------------ participant
function viewProfile(editing = false) {
  const p = ME.profile;
  if (p && !editing) {
    view.innerHTML = `<div class="row spread"><h1>${esc(p.name)}</h1><button class="btn" id="edit">Edit profile</button></div>
      <p class="muted">${esc([p.program, p.focus].filter(Boolean).join(" · "))}</p>
      <div class="grid-2">
        <div class="stack">
          ${p.summary ? `<div class="panel"><h3>Summary</h3><p>${esc(p.summary)}</p></div>` : ""}
          <div class="panel"><h3>Skills</h3>${chips(p.skills)}</div>
          ${p.experience ? `<div class="panel"><h3>Experience</h3><p style="white-space:pre-line">${esc(p.experience)}</p></div>` : ""}
        </div>
        <div class="panel"><h3>Wants to learn</h3>${p.wanted.length ? chips(p.wanted.map(n => ({ name: n }))) : `<p class="muted">Nothing added yet.</p>`}</div>
      </div>`;
    $("#edit").onclick = () => viewProfile(true);
    return;
  }
  const v = p || { name: "", program: "", focus: "", summary: "", experience: "", skills: [], wanted: [] };
  view.innerHTML = `<h1>${p ? "Edit profile" : "Set up your profile"}</h1>
    ${p ? "" : `<p class="muted">Hosts match people to projects by skill, so the more accurate this is, the better your matches.</p>`}
    <div class="progress" aria-label="Profile completion"><span id="prog"></span></div>
    <form id="pf" class="panel" novalidate>
      <div class="form-grid">
        <div class="field"><label for="name">Full name <span class="req">*</span></label><input id="name" maxlength="80" value="${esc(v.name)}"></div>
        <div class="field"><label for="program">Program and year <span class="req">*</span></label><input id="program" maxlength="80" placeholder="B.Tech CSE · 2nd year" value="${esc(v.program)}"></div>
        <div class="field"><label for="focus">Main area</label><input id="focus" maxlength="40" placeholder="Web, AI/ML, Data…" value="${esc(v.focus)}"></div>
      </div>
      <div class="field"><label for="summary">One-line summary</label><input id="summary" maxlength="280" placeholder="What you build and enjoy" value="${esc(v.summary)}"></div>
      <div class="field"><label>Skills <span class="req">*</span></label><div id="skills"></div></div>
      <div class="field"><label for="experience">Experience</label><textarea id="experience" maxlength="600" placeholder="Projects, internships, hackathons">${esc(v.experience)}</textarea></div>
      <div class="field"><label>Want to learn</label><div id="wanted"></div></div>
      <p class="error" id="pf-err" hidden></p>
      <div class="row"><button class="btn primary" id="save" type="submit">${p ? "Save changes" : "Finish setup"}</button>
      ${p ? `<button class="btn" type="button" id="cancel">Cancel</button>` : ""}</div>
    </form>`;
  const progress = () => {
    const done = [$("#name").value.trim(), $("#program").value.trim(), $("#summary").value.trim(),
      skills && skills.get().length, $("#experience").value.trim(), wanted && wanted.get().length].filter(Boolean).length;
    $("#prog").style.width = `${Math.round(done / 6 * 100)}%`;
  };
  let skills, wanted;
  skills = skillEditor($("#skills"), v.skills, { onChange: () => setTimeout(progress) });
  wanted = skillEditor($("#wanted"), v.wanted, { withLevel: false, onChange: () => setTimeout(progress) });
  $("#pf").addEventListener("input", progress);
  progress();
  if (p) $("#cancel").onclick = () => viewProfile(false);
  $("#pf").addEventListener("submit", e => {
    e.preventDefault();
    const err = $("#pf-err");
    const body = { name: $("#name").value, program: $("#program").value, focus: $("#focus").value,
      summary: $("#summary").value, experience: $("#experience").value, skills: skills.get(), wanted: wanted.get() };
    if (!body.name.trim() || !body.program.trim() || !body.skills.length) {
      err.textContent = "Add your name, program and at least one skill."; err.hidden = false; return;
    }
    act($("#save"), async () => {
      try {
        ME = await api("/api/profile", { method: "PUT", body });
        toast(p ? "Changes saved." : "Profile created.");
        if (p) viewProfile(false); else go("#/opportunities");
      } catch (e2) { err.textContent = e2.message; err.hidden = false; }
    });
  });
}

async function viewOpportunities() {
  if (!ME.profile) { view.innerHTML = `<div class="empty">Set up your profile to see your matches.<br><br><a class="btn primary" href="#/profile">Set up profile</a></div>`; return; }
  const { items } = await api("/api/opportunities");
  view.innerHTML = `<h1>Opportunities</h1><p class="muted">Sorted by how well your skills fit each project.</p>
    <div class="list">${items.map(p => `<article class="item" data-id="${esc(p.id)}">
      <div class="row spread"><div><h3>${esc(p.name)}</h3><span class="muted small">${esc(p.type)} · teams of ${p.minTeamSize}–${p.maxTeamSize}</span></div>${matchBadge(p.match)}</div>
      ${p.summary ? `<p style="margin:.4rem 0">${esc(p.summary)}</p>` : ""}
      <div class="chips" style="margin:.5rem 0">${p.why.map(r => chip(r.skill, r.required, r.status === "met" ? "met" : "")).join("")}</div>
      <p class="small muted">${esc(insights(p.why))}</p>
      <div class="row">
        <button class="btn" data-why>Why this match?</button>
        ${p.interest ? `<span class="pill ${p.interest}">${STATUS[p.interest]}</span>
          ${["interested", "under_review"].includes(p.interest) ? `<button class="btn ghost" data-withdraw>Withdraw</button>` : ""}`
        : `<button class="btn primary" data-interest>I'm interested</button>`}
      </div></article>`).join("")}</div>`;
  view.onclick = e => {
    const card = e.target.closest("[data-id]"); if (!card) return;
    const p = items.find(x => x.id === card.dataset.id);
    if (e.target.closest("[data-why]")) showWhy(p.name, p.match, p.why);
    const b = e.target.closest("[data-interest],[data-withdraw]");
    if (b) act(b, async () => {
      const add = b.hasAttribute("data-interest");
      await api(`/api/projects/${encodeURIComponent(p.id)}/interest`, { method: add ? "POST" : "DELETE", body: {} });
      toast(add ? `Interest sent to ${p.name}.` : "Interest withdrawn.");
      viewOpportunities();
    });
  };
}

// ------------------------------------------------------------ host
async function viewHost() {
  const { items } = await api("/api/projects");
  view.innerHTML = `<div class="row spread"><h1>Projects</h1><a class="btn primary" href="#/host/new">New project</a></div>
    ${items.length ? `<div class="list">${items.map(p => `<article class="item" data-id="${esc(p.id)}">
      <div class="row spread"><div><h3>${esc(p.name)}</h3><span class="muted small">${esc(p.type)} · teams of ${p.minTeamSize}–${p.maxTeamSize}</span></div>
        <span class="small"><b>${p.applicants}</b> interested · <b>${p.accepted}</b> accepted · <b>${p.teams}</b> teams</span></div>
      <div style="margin:.5rem 0">${chips(p.required)}</div>
      <div class="row">
        <a class="btn primary" href="#/host/team/${esc(p.id)}">Build team</a>
        <a class="btn" href="#/host/applicants/${esc(p.id)}">Applicants</a>
        <a class="btn" href="#/host/edit/${esc(p.id)}">Edit</a>
        <button class="btn danger" data-del>Delete</button>
      </div></article>`).join("")}</div>`
      : `<div class="empty">No projects yet. Create one to start matching participants.</div>`}`;
  view.onclick = e => {
    const b = e.target.closest("[data-del]"); if (!b) return;
    const p = items.find(x => x.id === b.closest("[data-id]").dataset.id);
    const body = openModal(`Delete ${p.name}?`, `<p>This removes the project, its interest requests and saved teams. It can't be undone.</p>
      <div class="row"><button class="btn danger" id="yes">Delete project</button><button class="btn" data-close>Keep it</button></div>`);
    $("#yes", body).onclick = ev => act(ev.target, async () => {
      await api(`/api/projects/${encodeURIComponent(p.id)}`, { method: "DELETE", body: {} });
      closeModal(); toast("Project deleted."); viewHost();
    });
  };
}

async function viewProjectForm(id) {
  const p = id ? await api(`/api/projects/${encodeURIComponent(id)}`)
    : { name: "", type: PROJECT_TYPES[0], summary: "", minTeamSize: 2, maxTeamSize: 4, required: [] };
  const types = PROJECT_TYPES.includes(p.type) ? PROJECT_TYPES : [p.type, ...PROJECT_TYPES];
  view.innerHTML = `<h1>${id ? "Edit project" : "New project"}</h1>
    <form id="pj" class="panel" novalidate>
      <div class="form-grid">
        <div class="field"><label for="name">Project name <span class="req">*</span></label><input id="name" maxlength="80" value="${esc(p.name)}"></div>
        <div class="field"><label for="type">Type <span class="req">*</span></label><select id="type">${types.map(t => `<option ${t === p.type ? "selected" : ""}>${esc(t)}</option>`).join("")}</select></div>
        <div class="field"><label for="min">Min team size</label><input id="min" type="number" min="1" max="20" value="${p.minTeamSize}"></div>
        <div class="field"><label for="max">Max team size</label><input id="max" type="number" min="1" max="20" value="${p.maxTeamSize}"></div>
      </div>
      <div class="field"><label for="summary">Summary</label><input id="summary" maxlength="300" value="${esc(p.summary)}"></div>
      <div class="field"><label>Required skills and minimum levels <span class="req">*</span></label><div id="req"></div></div>
      <p class="error" id="pj-err" hidden></p>
      <div class="row"><button class="btn primary" id="save" type="submit">${id ? "Save changes" : "Create project"}</button><a class="btn" href="#/host">Cancel</a></div>
    </form>`;
  const req = skillEditor($("#req"), p.required, { levelLabel: "Minimum level" });
  $("#pj").addEventListener("submit", e => {
    e.preventDefault();
    const err = $("#pj-err");
    const body = { name: $("#name").value, type: $("#type").value, summary: $("#summary").value,
      minTeamSize: parseInt($("#min").value, 10), maxTeamSize: parseInt($("#max").value, 10), required: req.get() };
    act($("#save"), async () => {
      try {
        await api(id ? `/api/projects/${encodeURIComponent(id)}` : "/api/projects", { method: id ? "PUT" : "POST", body });
        toast(id ? "Changes saved." : "Project created."); go("#/host");
      } catch (e2) { err.textContent = e2.message; err.hidden = false; }
    });
  });
}

async function viewTeamBuilder(id) {
  const project = await api(`/api/projects/${encodeURIComponent(id)}`);
  let members = [], pool = "all", candidates = [], evalRes = null;

  view.innerHTML = `<a href="#/host" class="small">All projects</a>
    <h1>${esc(project.name)}</h1>
    <p class="muted">Build a team of ${project.minTeamSize}–${project.maxTeamSize}. Add people yourself or let TeamForge suggest a team that covers the most skills.</p>
    <div class="grid-2">
      <section>
        <div class="row spread" style="margin-bottom:.6rem"><h2 style="margin:0">Best matches</h2>
          <select id="pool" style="width:auto" aria-label="Who to match from">
            <option value="all">Everyone</option><option value="accepted">Accepted applicants</option></select></div>
        <div id="cands" class="list"></div>
      </section>
      <aside class="sticky stack">
        <div class="panel" id="team"></div>
        <div class="panel"><h3>Saved teams</h3><div id="saved"></div></div>
      </aside>
    </div>`;

  async function loadCands() {
    candidates = (await api(`/api/projects/${encodeURIComponent(id)}/matches?limit=40&pool=${pool}`)).items;
    drawCands();
  }
  function drawCands() {
    $("#cands").innerHTML = candidates.length ? candidates.map(c => `<article class="item" data-sid="${esc(c.id)}">
      <div class="row spread"><div><h3>${esc(c.name)}</h3><span class="muted small">${esc(c.program)}</span></div>${matchBadge(c.match)}</div>
      <div style="margin:.45rem 0">${chips(c.skills)}</div>
      <div class="row"><button class="btn" data-why>Why this match?</button>
        ${members.includes(c.id) ? `<button class="btn" data-rm>Remove</button>` : `<button class="btn primary" data-add>Add</button>`}</div>
    </article>`).join("") : `<div class="empty">${pool === "accepted" ? "No accepted applicants yet. Review them on the Applicants page." : "Nobody has any of the required skills yet."}</div>`;
  }
  async function evaluate() {
    evalRes = await api(`/api/projects/${encodeURIComponent(id)}/evaluate`, { method: "POST", body: { members } });
    drawTeam();
  }
  function drawTeam() {
    const r = evalRes;
    $("#team").innerHTML = `<div class="row spread"><h2 style="margin:0">Your team</h2>${matchBadge(r.coverage.percent)}</div>
      <p class="small ${r.sizeOk ? "muted" : "error"}">${r.size} selected. ${esc(r.sizeMessage)}</p>
      ${r.members.length ? `<div class="list" style="margin-bottom:.8rem">${r.members.map(mm => `<div class="row spread" data-sid="${esc(mm.id)}">
          <span><b>${esc(mm.name)}</b> <span class="muted small">${mm.match}%</span></span><button class="icon-btn" data-rm aria-label="Remove ${esc(mm.name)}">✕</button></div>`).join("")}</div>` : ""}
      <h3>Skill coverage</h3>${ladder(r.coverage.skills)}
      ${r.coverage.missing.length ? `<p class="small"><b>Still missing:</b> ${r.coverage.missing.map(s => esc(s.toUpperCase())).join(", ")}</p>` : `<p class="small"><b>Every required skill is covered.</b></p>`}
      <div class="row" style="margin:.6rem 0"><button class="btn" id="suggest">Suggest team</button><button class="btn ghost" id="clear">Clear</button></div>
      <div class="field" style="margin-bottom:.5rem"><label for="tname">Team name</label><input id="tname" maxlength="60" placeholder="Team ${esc(project.name)} A"></div>
      <button class="btn primary" id="saveTeam" ${r.sizeOk ? "" : "disabled"}>Save team</button>`;
  }
  async function loadSaved() {
    const { items } = await api(`/api/projects/${encodeURIComponent(id)}/teams`);
    $("#saved").innerHTML = items.length ? items.map(t => `<div class="item" style="margin-top:.5rem" data-tid="${t.id}">
      <div class="row spread"><b>${esc(t.name)}</b>${matchBadge(t.coverage.percent)}</div>
      <p class="small muted" style="margin:.3rem 0">${t.members.map(x => esc(x.name)).join(", ")}</p>
      <button class="btn ghost danger" data-deltm>Delete team</button></div>`).join("") : `<p class="muted small">No teams saved yet.</p>`;
  }

  view.onclick = e => {
    const sidEl = e.target.closest("[data-sid]");
    const sid = sidEl && sidEl.dataset.sid;
    if (e.target.closest("[data-why]")) { const c = candidates.find(x => x.id === sid); showWhy(c.name, c.match, c.why); }
    if (e.target.closest("[data-add]")) {
      if (members.length >= project.maxTeamSize) return toast(`This project allows at most ${project.maxTeamSize} people.`, true);
      members.push(sid); drawCands(); act(null, evaluate);
    }
    if (e.target.closest("[data-rm]")) { members = members.filter(x => x !== sid); drawCands(); act(null, evaluate); }
    if (e.target.id === "clear") { members = []; drawCands(); act(null, evaluate); }
    if (e.target.id === "suggest") act(e.target, async () => {
      evalRes = await api(`/api/projects/${encodeURIComponent(id)}/suggest`, { method: "POST", body: { members, pool } });
      members = evalRes.members.map(x => x.id);
      drawTeam(); drawCands();
      toast("Suggested a team. Review it before saving.");
    });
    if (e.target.id === "saveTeam") act(e.target, async () => {
      const name = $("#tname").value.trim() || $("#tname").placeholder;
      await api(`/api/projects/${encodeURIComponent(id)}/teams`, { method: "POST", body: { name, members } });
      toast(`Saved ${name}.`);
      members = []; drawCands(); await evaluate(); await loadSaved();
    });
    const del = e.target.closest("[data-deltm]");
    if (del) act(del, async () => {
      await api(`/api/teams/${del.closest("[data-tid]").dataset.tid}`, { method: "DELETE", body: {} });
      toast("Team deleted."); loadSaved();
    });
  };
  $("#pool").onchange = e => { pool = e.target.value; act(null, loadCands); };
  await Promise.all([loadCands(), evaluate(), loadSaved()]);
}

async function viewApplicants(id) {
  const [project, { items }] = await Promise.all([
    api(`/api/projects/${encodeURIComponent(id)}`), api(`/api/projects/${encodeURIComponent(id)}/applicants`)]);
  view.innerHTML = `<a href="#/host" class="small">All projects</a><h1>Applicants · ${esc(project.name)}</h1>
    <p class="muted">Accepted applicants can be used as the pool in the team builder.</p>
    ${items.length ? `<div class="list">${items.map(a => `<article class="item" data-iid="${a.interestId}" data-sid="${esc(a.id)}">
      <div class="row spread"><div><h3>${esc(a.name)}</h3><span class="muted small">${esc(a.program)}</span></div>
        <div class="row">${matchBadge(a.match)}<span class="pill ${a.status}">${STATUS[a.status]}</span></div></div>
      <div style="margin:.45rem 0">${chips(a.skills)}</div>
      <div class="row"><button class="btn" data-why>Why this match?</button><button class="btn" data-profile>View profile</button>
        ${a.status !== "accepted" ? `<button class="btn primary" data-st="accepted">Accept</button>` : ""}
        ${a.status !== "declined" ? `<button class="btn danger" data-st="declined">Decline</button>` : ""}
        ${a.status === "interested" ? `<button class="btn ghost" data-st="under_review">Mark under review</button>` : ""}
      </div></article>`).join("")}</div>` : `<div class="empty">Nobody has shown interest in this project yet.</div>`}`;
  view.onclick = e => {
    const card = e.target.closest("[data-iid]"); if (!card) return;
    const a = items.find(x => String(x.interestId) === card.dataset.iid);
    if (e.target.closest("[data-why]")) showWhy(a.name, a.match, a.why);
    if (e.target.closest("[data-profile]")) showStudent(a.id);
    const b = e.target.closest("[data-st]");
    if (b) act(b, async () => {
      await api(`/api/interests/${a.interestId}`, { method: "PATCH", body: { status: b.dataset.st } });
      toast(`${a.name}: ${STATUS[b.dataset.st]}.`); viewApplicants(id);
    });
  };
}

async function showStudent(sid) {
  const s = await api(`/api/students/${encodeURIComponent(sid)}`).catch(e => (toast(e.message, true), null));
  if (!s) return;
  openModal(s.name, `<p class="muted">${esc([s.program, s.focus, s.id].filter(Boolean).join(" · "))}</p>
    ${s.summary ? `<p>${esc(s.summary)}</p>` : ""}
    <h3>Skills</h3>${chips(s.skills)}
    ${s.experience ? `<h3 style="margin-top:1rem">Experience</h3><p style="white-space:pre-line">${esc(s.experience)}</p>` : ""}
    ${s.wanted.length ? `<h3 style="margin-top:1rem">Wants to learn</h3>${chips(s.wanted.map(n => ({ name: n })))}` : ""}
    ${s.interests.length ? `<h3 style="margin-top:1rem">Interested in</h3>${s.interests.map(i => `<div class="row spread small"><span>${esc(i.name)}</span><span class="pill ${i.status}">${STATUS[i.status]}</span></div>`).join("")}` : ""}`);
}

async function viewPeople() {
  const { items: projects } = await api("/api/projects");
  const st = { q: "", skill: "", minLevel: 1, year: "", sort: "name", project: "", page: 1 };
  view.innerHTML = `<h1>Participants</h1>
    <div class="filters">
      <input id="q" placeholder="Search name, ID or summary" aria-label="Search">
      <div><input id="skill" placeholder="Has skill…" aria-label="Filter by skill"></div>
      <select id="minLevel" aria-label="Minimum level">${[1, 2, 3, 4, 5].map(l => `<option value="${l}">Level ${l}+</option>`).join("")}</select>
      <select id="year" aria-label="Year"><option value="">Any year</option>${[1, 2, 3, 4].map(y => `<option value="${y}">Year ${y}</option>`).join("")}</select>
      <select id="project" aria-label="Match against project"><option value="">No project</option>${projects.map(p => `<option value="${esc(p.id)}">${esc(p.name)}</option>`).join("")}</select>
      <select id="sort" aria-label="Sort"><option value="name">Sort: name</option><option value="id">Sort: ID</option><option value="skills">Sort: most skilled</option><option value="match">Sort: best match</option></select>
    </div>
    <p class="small muted" id="count"></p>
    <div class="panel scroll-x"><table><thead><tr><th>#</th><th>Name</th><th>ID</th><th>Program</th><th>Skills</th><th id="mh">Match</th></tr></thead><tbody id="rows"></tbody></table></div>
    <div class="row" style="margin-top:.75rem"><button class="btn" id="prev">Previous</button><button class="btn" id="next">Next</button></div>`;
  let total = 0;
  const load = async () => {
    const qs = new URLSearchParams(Object.entries(st).filter(([, v]) => v !== "" && v != null)).toString();
    const r = await api(`/api/students?${qs}&per=25`);
    total = r.total;
    $("#count").textContent = `${r.total} participants${r.total ? ` · showing ${(r.page - 1) * r.per + 1}–${Math.min(r.page * r.per, r.total)}` : ""}`;
    $("#mh").hidden = !st.project;
    $("#rows").innerHTML = r.items.length ? r.items.map(s => `<tr data-sid="${esc(s.id)}" style="cursor:pointer">
      <td class="muted">${s.number}</td><td><b>${esc(s.name)}</b></td><td class="muted small nowrap">${esc(s.id)}</td>
      <td class="small">${esc(s.program)}</td><td>${chips(s.skills.slice(0, 4))}</td>
      ${st.project ? `<td>${matchBadge(s.match)}</td>` : ""}</tr>`).join("")
      : `<tr><td colspan="6"><div class="empty">No participants match these filters. Try removing one.</div></td></tr>`;
    $("#prev").disabled = st.page <= 1;
    $("#next").disabled = st.page * 25 >= total;
  };
  const reload = () => { st.page = 1; act(null, load); };
  $("#q").addEventListener("input", debounce(e => { st.q = e.target.value; reload(); }, 250));
  autocomplete($("#skill"), name => { st.skill = name; $("#skill").value = name; setTimeout(() => ($("#skill").value = name)); reload(); });
  $("#skill").addEventListener("input", e => { if (!e.target.value) { st.skill = ""; reload(); } });
  ["minLevel", "year", "sort", "project"].forEach(k => $("#" + k).addEventListener("change", e => {
    st[k] = e.target.value;
    if (k === "sort" && st.sort === "match" && !st.project) toast("Pick a project to sort by match.", true);
    reload();
  }));
  $("#prev").onclick = () => { st.page--; act(null, load); };
  $("#next").onclick = () => { st.page++; act(null, load); };
  $("#rows").onclick = e => { const tr = e.target.closest("[data-sid]"); if (tr) showStudent(tr.dataset.sid); };
  await load();
}

// ------------------------------------------------------------ admin
async function viewAdmin() {
  const [s, { items: users }] = await Promise.all([api("/api/admin/stats"), api("/api/admin/users")]);
  view.innerHTML = `<h1>Overview</h1>
    <div class="stats">${[["Participants", s.participants], ["Projects", s.projects], ["Skills", s.skills],
      ["Accounts", s.users], ["Interest requests", s.interests], ["Saved teams", s.teams]]
      .map(([l, n]) => `<div class="stat"><b>${n}</b><span class="muted small">${l}</span></div>`).join("")}</div>
    <div class="panel" style="margin-top:1rem"><div class="row spread"><h2 style="margin:0">Data checks</h2><button class="btn" id="validate">Run checks</button></div><div id="checks"></div></div>
    <div class="panel" style="margin-top:1rem"><h2>Accounts</h2><p class="small muted">Make someone a host so they can create projects and build teams.</p>
      <div class="scroll-x"><table><thead><tr><th>Email</th><th>Name</th><th>Joined</th><th>Role</th></tr></thead><tbody>
      ${users.map(u => `<tr><td>${esc(u.email)}</td><td>${esc(u.name || "—")}</td><td class="small muted">${esc(u.created_at.slice(0, 10))}</td>
        <td><select data-uid="${u.id}" aria-label="Role for ${esc(u.email)}" ${u.id === ME.user.id ? "disabled" : ""}>
        ${["participant", "host", "admin"].map(r => `<option ${r === u.role ? "selected" : ""}>${r}</option>`).join("")}</select></td></tr>`).join("")}
      </tbody></table></div></div>`;
  $("#validate").onclick = e => act(e.target, async () => {
    const r = await api("/api/admin/validate");
    $("#checks").innerHTML = `<table><tbody>${r.items.map(c => `<tr><td>${esc(c.check)}</td><td><span class="pill ${c.problems ? "declined" : "accepted"}">${c.problems ? `${c.problems} found` : "OK"}</span></td></tr>`).join("")}</tbody></table>`;
  });
  view.onchange = e => {
    const sel = e.target.closest("[data-uid]"); if (!sel) return;
    act(sel, async () => { await api(`/api/admin/users/${sel.dataset.uid}`, { method: "PATCH", body: { role: sel.value } }); toast(`Role changed to ${sel.value}.`); });
  };
}

async function viewSkills() {
  let { items, categories } = await api("/api/admin/skills");
  const st = { q: "", cat: "", sort: "name" };
  view.innerHTML = `<h1>Skills</h1><p class="muted">Skills are open-ended: anyone can add a new one. Use this page to categorise, rename or merge them.</p>
    <form id="add" class="panel row" style="margin-bottom:1rem" novalidate>
      <input id="nname" placeholder="New skill name" maxlength="40" style="flex:2 1 180px" aria-label="New skill name">
      <input id="ncat" placeholder="Category" maxlength="40" list="cats" style="flex:1 1 140px" aria-label="Category">
      <datalist id="cats">${categories.map(c => `<option value="${esc(c)}">`).join("")}</datalist>
      <button class="btn primary" id="addbtn">Add skill</button></form>
    <div class="filters"><input id="q" placeholder="Search skills" aria-label="Search skills">
      <select id="cat" aria-label="Category"><option value="">All categories</option>${categories.map(c => `<option>${esc(c)}</option>`).join("")}</select>
      <select id="sort" aria-label="Sort"><option value="name">Sort: name</option><option value="people">Sort: most used</option></select></div>
    <p class="small muted" id="count"></p>
    <div class="panel scroll-x"><table><thead><tr><th>Skill</th><th>Category</th><th>People</th><th>Projects</th><th></th></tr></thead><tbody id="rows"></tbody></table></div>`;
  const draw = () => {
    let list = items.filter(s => (!st.q || s.name.includes(st.q.toLowerCase())) && (!st.cat || s.category === st.cat));
    if (st.sort === "people") list = [...list].sort((a, b) => b.people - a.people);
    $("#count").textContent = `${list.length} of ${items.length} skills`;
    $("#rows").innerHTML = list.slice(0, 200).map(s => `<tr data-name="${esc(s.name)}"><td>${chip(s.name)}</td><td class="small">${esc(s.category)}</td>
      <td>${s.people}</td><td>${s.projects}</td><td class="row"><button class="btn ghost" data-edit>Edit</button>
      <button class="btn ghost danger" data-del>Delete</button></td></tr>`).join("")
      + (list.length > 200 ? `<tr><td colspan="5" class="muted small">Showing the first 200. Search to narrow down.</td></tr>` : "");
  };
  const reload = async () => { ({ items, categories } = await api("/api/admin/skills")); draw(); };
  $("#q").oninput = e => { st.q = e.target.value; draw(); };
  $("#cat").onchange = e => { st.cat = e.target.value; draw(); };
  $("#sort").onchange = e => { st.sort = e.target.value; draw(); };
  $("#add").onsubmit = e => {
    e.preventDefault();
    act($("#addbtn"), async () => {
      await api("/api/admin/skills", { method: "POST", body: { name: $("#nname").value, category: $("#ncat").value } });
      toast("Skill added."); $("#nname").value = ""; await reload();
    });
  };
  $("#rows").onclick = e => {
    const tr = e.target.closest("[data-name]"); if (!tr) return;
    const s = items.find(x => x.name === tr.dataset.name);
    if (e.target.closest("[data-del]")) act(e.target, async () => {
      await api(`/api/admin/skills/${encodeURIComponent(s.name)}`, { method: "DELETE", body: {} });
      toast("Skill deleted."); await reload();
    });
    if (e.target.closest("[data-edit]")) {
      const body = openModal(`Edit ${s.name.toUpperCase()}`, `
        <div class="field"><label for="ename">Name</label><input id="ename" maxlength="40" value="${esc(s.name)}">
          <div class="hint">Renaming to an existing skill merges the two. Each person keeps their higher level.</div></div>
        <div class="field"><label for="ecat">Category</label><input id="ecat" maxlength="40" list="cats" value="${esc(s.category)}"></div>
        <p class="small muted">Used by ${s.people} people and ${s.projects} projects.</p>
        <button class="btn primary" id="esave">Save changes</button>`);
      $("#esave", body).onclick = ev => act(ev.target, async () => {
        await api(`/api/admin/skills/${encodeURIComponent(s.name)}`, { method: "PATCH",
          body: { newName: $("#ename", body).value, category: $("#ecat", body).value } });
        closeModal(); toast("Changes saved."); await reload();
      });
    }
  };
  draw();
}

// ------------------------------------------------------------ start
(async () => {
  try { ME = await api("/api/me"); } catch { ME = { user: null, profile: null }; }
  render();
})();
