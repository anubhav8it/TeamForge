"use strict";
// Forgot password, choose a new password, confirm email (forgot.html, reset.html, verify.html).
// The one-time token arrives in the link's ?token=; it is removed from the address bar at once
// and only ever sent to this server in a POST body.
(function () {
  const $ = id => document.getElementById(id);
  const message = $("message");

  function show(text, kind, field) {
    if (!message) return;
    message.textContent = text;
    message.className = "message " + (kind || "error");
    if (field) { field.setAttribute("aria-invalid", "true"); field.focus(); }
  }
  for (const input of document.querySelectorAll(".field input"))
    input.addEventListener("input", () => input.removeAttribute("aria-invalid"));

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
  function busy(button, on, text) {
    button.disabled = on;
    button.classList.toggle("loading", on);
    if (on) { button.dataset.label = button.textContent; button.textContent = text; button.setAttribute("aria-busy", "true"); }
    else { button.textContent = button.dataset.label || button.textContent; button.removeAttribute("aria-busy"); }
  }

  const token = new URLSearchParams(location.search).get("token") || "";
  if (token) history.replaceState(null, "", location.pathname);

  // ---- forgot.html
  const forgot = $("form-forgot");
  if (forgot) {
    const email = $("forgot-email");
    if (location.hash.length > 1) email.value = decodeURIComponent(location.hash.slice(1));
    forgot.addEventListener("submit", async event => {
      event.preventDefault();
      const value = email.value.trim();
      if (!/^[^@\s]+@[^@\s]+\.[^@\s]{2,}$/.test(value)) return show("Enter a valid email address.", "error", email);
      const button = forgot.querySelector("button[type=submit]");
      busy(button, true, "Sending…");
      try {
        const data = await post("/api/password/forgot", { email: value });
        show(data.message, "info");
        forgot.classList.add("hidden");
      } catch (error) {
        show(error.message);
      }
      busy(button, false);
    });
  }

  // ---- reset.html
  const reset = $("form-reset");
  if (reset) {
    const expired = () => {
      reset.classList.add("hidden");
      $("reset-title").textContent = "This link has expired";
      $("reset-sub").textContent = "Reset links work once and only for 30 minutes. Ask for a new one.";
    };
    if (!token) expired();
    else post("/api/password/check", { token }).then(data => { if (!data.valid) expired(); }).catch(() => {});
    reset.addEventListener("submit", async event => {
      event.preventDefault();
      const password = $("reset-password").value;
      if (password.length < 8) return show("Use a password of at least 8 characters.", "error", $("reset-password"));
      if (password !== $("reset-confirm").value) return show("The two passwords do not match.", "error", $("reset-confirm"));
      const button = reset.querySelector("button[type=submit]");
      busy(button, true, "Saving…");
      try {
        await post("/api/password/reset", { token, password });
        location.assign("/login?reason=reset");
      } catch (error) {
        show(error.message);
        busy(button, false);
      }
    });
  }

  // ---- verify.html
  const verify = $("verify-button");
  if (verify) {
    const title = $("verify-title"), sub = $("verify-sub");
    if (!token) {
      title.textContent = "This link is incomplete";
      sub.textContent = "Open the link from your email again, or sign in and ask for a new one.";
      verify.classList.add("hidden");
    } else {
      title.textContent = "Confirm your email";
      sub.textContent = "Press the button to confirm that this email address is yours.";
      verify.addEventListener("click", async () => {
        busy(verify, true, "Confirming…");
        try {
          await post("/api/verify-email", { token });
          title.textContent = "Email confirmed";
          sub.textContent = "Thanks! Password resets and team updates will reach you.";
          verify.classList.add("hidden");
        } catch (error) {
          title.textContent = "This link did not work";
          sub.textContent = error.message;
          busy(verify, false);
          verify.classList.add("hidden");
        }
      });
    }
  }
})();
