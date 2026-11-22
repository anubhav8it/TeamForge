// The public team invitation page (/invite/<token>). Shows the team's public details; signed-out
// visitors sign in or register and come back here (?next=); signed-in visitors continue into the
// app, which shows their compatibility with the team and the application form. The token stays
// in the path only; it is sent to the server in POST bodies, never in a query string.
(function () {
  "use strict";
  const $ = id => document.getElementById(id);
  const token = (location.pathname.match(/^\/invite\/([A-Za-z0-9_-]{20,64})$/) || [])[1];

  async function post(path, body) {
    const res = await fetch(path, {
      method: "POST", credentials: "same-origin",
      headers: { "Content-Type": "application/json", "X-TeamForge": "1" },
      body: JSON.stringify(body),
    });
    let data = {};
    try { data = await res.json(); } catch (e) { /* empty */ }
    if (!res.ok) throw new Error(data.error || "Something went wrong (" + res.status + ").");
    return data;
  }

  function fail(text) {
    $("loading").classList.add("hidden");
    $("team").classList.add("hidden");
    const box = $("message");
    box.textContent = text;
    box.className = "message error";
  }

  function chips(list, items) {
    list.textContent = "";
    for (const text of items) {
      const li = document.createElement("li");
      li.className = "chip";
      li.textContent = text;
      list.appendChild(li);
    }
  }

  function initials(name) {
    const parts = String(name || "").trim().split(/\s+/).filter(Boolean);
    return ((parts[0] || "?")[0] + (parts.length > 1 ? parts[parts.length - 1][0] : (parts[0] || "")[1] || "")).toUpperCase();
  }

  const LEVELS = ["", "any level", "basic", "intermediate", "advanced", "expert"];

  function render(team) {
    $("team-name").textContent = team.name;
    $("team-tagline").textContent = team.tagline || "";
    $("team-mark").textContent = initials(team.name);
    $("team-leader").textContent = team.leader ? team.leader.name : "–";
    $("team-vacancies").textContent = team.full ? "Full right now" : team.vacancies + " of " + team.maxMembers;
    $("team-description").textContent = team.description || team.objectives || "";
    const skills = Object.entries(team.requiredSkills || {});
    $("skills-block").classList.toggle("hidden", skills.length === 0);
    chips($("team-skills"), skills.map(([name, level]) => level > 1 ? name + " · " + LEVELS[level] : name));
    $("roles-block").classList.toggle("hidden", !(team.openRoles || []).length);
    chips($("team-roles"), team.openRoles || []);
    document.title = "TeamForge – Join " + team.name;
    $("loading").classList.add("hidden");
    $("team").classList.remove("hidden");
  }

  async function start() {
    if (!token) return fail("This invitation link is not complete. Copy the whole link and try again.");
    let preview;
    try {
      preview = await post("/api/invite/preview", { token });
    } catch (error) {
      return fail(error.message);
    }
    render(preview.team);
    const me = await fetch("/api/me", { credentials: "same-origin" }).then(r => r.json()).catch(() => ({}));
    const next = "?next=" + encodeURIComponent("/invite/" + token);
    if (!me.user) {
      $("signin").href = "/login" + next;
      $("register").href = "/login" + next + "#register";
      $("signed-out").classList.remove("hidden");
      return;
    }
    $("signed-in").classList.remove("hidden");
    $("continue").addEventListener("click", async () => {
      const button = $("continue");
      button.disabled = true;
      button.textContent = "Opening TeamForge…";
      try {
        await post("/api/hub/invite-link/claim", { token });
        location.assign("/app/");
      } catch (error) {
        fail(error.message);
      }
    });
  }

  start();
})();
