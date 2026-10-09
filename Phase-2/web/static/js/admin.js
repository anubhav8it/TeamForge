"use strict";
(function () {
  const $ = id => document.getElementById(id);
  const message = $("message");
  const ROLES = ["participant", "host", "admin"];

  function show(text, kind) { message.textContent = text; message.className = "message " + (kind || "error"); }

  async function api(path, method, body) {
    const options = { method: method || "GET", credentials: "same-origin", headers: {} };
    if (body !== undefined) {
      options.headers["Content-Type"] = "application/json";
      options.headers["X-TeamForge"] = "1";
      options.body = JSON.stringify(body);
    }
    const res = await fetch(path, options);
    let data = {};
    try { data = await res.json(); } catch (e) { /* empty */ }
    if (res.status === 401) { location.assign("/login?reason=expired"); throw new Error("Signed out"); }
    if (!res.ok) throw new Error(data.error || "Request failed (" + res.status + ").");
    return data;
  }

  const when = t => (t ? new Date(t * 1000).toLocaleString() : "—");

  function cell(text) { const td = document.createElement("td"); td.textContent = text; return td; }

  async function load() {
    const data = await api("/api/admin/users");
    const body = $("users");
    body.textContent = "";
    for (const user of data.users) {
      const tr = document.createElement("tr");
      tr.appendChild(cell(user.email + (user.id === data.me ? "  (you)" : "")));
      const roleCell = document.createElement("td");
      const select = document.createElement("select");
      select.setAttribute("aria-label", "Role for " + user.email);
      for (const role of ROLES) {
        const option = document.createElement("option");
        option.value = role; option.textContent = role[0].toUpperCase() + role.slice(1);
        option.selected = role === user.role;
        select.appendChild(option);
      }
      select.addEventListener("change", async () => {
        select.disabled = true;
        try {
          await api("/api/admin/users/" + user.id + "/role", "POST", { role: select.value });
          show(user.email + " is now " + select.value + ". They get the new workspaces the next time TeamForge loads.", "info");
        } catch (error) {
          show(error.message);
          select.value = user.role;
        }
        select.disabled = false;
        load().catch(e => show(e.message));
      });
      roleCell.appendChild(select);
      tr.appendChild(roleCell);
      tr.appendChild(cell(when(user.createdAt)));
      tr.appendChild(cell(when(user.lastLogin)));
      body.appendChild(tr);
    }
  }

  $("reset").addEventListener("click", async () => {
    try {
      await api("/api/admin/reset-data", "POST", { confirm: $("reset-confirm").value.trim() });
      $("reset-confirm").value = "";
      show("Shared data reset to the sample data. Everyone sees it after reloading TeamForge.", "info");
    } catch (error) { show(error.message); }
  });

  $("signout").addEventListener("click", async () => {
    try { await api("/api/logout", "POST", {}); } catch (e) { /* ignore */ }
    location.assign("/login");
  });

  load().catch(error => show(error.message));
})();
