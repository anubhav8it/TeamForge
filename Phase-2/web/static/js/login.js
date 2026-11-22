"use strict";
// Sign-in / create-account page. Real <form> elements with autocomplete hints, so the browser's
// password manager offers to save the email and password and fills them in next time.
(function () {
  const $ = id => document.getElementById(id);
  const message = $("message");
  const forms = { signin: $("form-signin"), register: $("form-register") };
  const tabs = { signin: $("tab-signin"), register: $("tab-register") };

  // `field`: the input to fix, marked invalid (and focused) until it changes.
  function show(text, kind, field) {
    message.textContent = text;
    message.className = "message " + (kind || "error");
    if (field) { field.setAttribute("aria-invalid", "true"); field.focus(); }
  }
  function clear() {
    message.className = "message hidden"; message.textContent = "";
    for (const input of document.querySelectorAll("input[aria-invalid]")) input.removeAttribute("aria-invalid");
  }
  for (const input of document.querySelectorAll(".field input"))
    input.addEventListener("input", () => input.removeAttribute("aria-invalid"));

  function select(which) {
    for (const key of Object.keys(forms)) {
      forms[key].classList.toggle("hidden", key !== which);
      tabs[key].setAttribute("aria-selected", String(key === which));
    }
    tabs.signin.parentElement.dataset.active = which; // slides the selection pill
    clear();
    (which === "signin" ? $("signin-email") : $("register-email")).focus();
  }
  tabs.signin.addEventListener("click", () => select("signin"));
  tabs.register.addEventListener("click", () => select("register"));
  if (location.hash === "#register") select("register");
  const reason = new URLSearchParams(location.search).get("reason");
  if (reason === "expired") show("Your session ended. Please sign in again.", "info");
  if (reason === "reset") show("Your password was changed. Sign in with your new password.", "info");
  if (reason === "verified") show("Thanks, your email address is confirmed. Sign in to continue.", "info");
  // Carry a typed email over to the Forgot password page.
  const forgot = $("forgot-link");
  if (forgot) forgot.addEventListener("click", () => {
    const email = $("signin-email").value.trim();
    if (email) forgot.href = "/forgot#" + encodeURIComponent(email);
  });

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

  async function submit(form, path, body) {
    const button = form.querySelector("button[type=submit]");
    const label = button.textContent;
    button.disabled = true;
    button.classList.add("loading");
    button.setAttribute("aria-busy", "true");
    button.textContent = form === forms.signin ? "Signing in…" : "Creating account…";
    clear();
    try {
      await post(path, body);
      // Back to the team invitation that sent the person here, if any (only /invite/ pages).
      const next = new URLSearchParams(location.search).get("next") || "";
      location.assign(/^\/invite\/[A-Za-z0-9_-]{20,64}$/.test(next) ? next : "/app/");
    } catch (error) {
      show(error.message);
      button.disabled = false;
      button.classList.remove("loading");
      button.removeAttribute("aria-busy");
      button.textContent = label;
    }
  }

  forms.signin.addEventListener("submit", event => {
    event.preventDefault();
    const email = $("signin-email").value.trim();
    const password = $("signin-password").value;
    if (!email || !password) return show("Enter your email and password.", "error", !email ? $("signin-email") : $("signin-password"));
    submit(forms.signin, "/api/login", { email, password });
  });

  forms.register.addEventListener("submit", event => {
    event.preventDefault();
    const email = $("register-email").value.trim();
    const password = $("register-password").value;
    if (!email) return show("Enter your email address.", "error", $("register-email"));
    if (password.length < 8) return show("Use a password of at least 8 characters.", "error", $("register-password"));
    if (password !== $("register-confirm").value) return show("The two passwords do not match.", "error", $("register-confirm"));
    submit(forms.register, "/api/register", { email, password });
  });
})();
