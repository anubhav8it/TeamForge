"use strict";
// Sign-in / create-account page. Real <form> elements with autocomplete hints, so the browser's
// password manager offers to save the email and password and fills them in next time.
(function () {
  const $ = id => document.getElementById(id);
  const message = $("message");
  const forms = { signin: $("form-signin"), register: $("form-register") };
  const tabs = { signin: $("tab-signin"), register: $("tab-register") };

  function show(text, kind) {
    message.textContent = text;
    message.className = "message " + (kind || "error");
  }
  function clear() { message.className = "message hidden"; message.textContent = ""; }

  function select(which) {
    for (const key of Object.keys(forms)) {
      forms[key].classList.toggle("hidden", key !== which);
      tabs[key].setAttribute("aria-selected", String(key === which));
    }
    clear();
    (which === "signin" ? $("signin-email") : $("register-email")).focus();
  }
  tabs.signin.addEventListener("click", () => select("signin"));
  tabs.register.addEventListener("click", () => select("register"));
  if (location.hash === "#register") select("register");
  const reason = new URLSearchParams(location.search).get("reason");
  if (reason === "expired") show("Your session ended. Please sign in again.", "info");

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
    button.disabled = true;
    clear();
    try {
      await post(path, body);
      location.assign("/app/");
    } catch (error) {
      show(error.message);
      button.disabled = false;
    }
  }

  forms.signin.addEventListener("submit", event => {
    event.preventDefault();
    const email = $("signin-email").value.trim();
    const password = $("signin-password").value;
    if (!email || !password) return show("Enter your email and password.");
    submit(forms.signin, "/api/login", { email, password });
  });

  forms.register.addEventListener("submit", event => {
    event.preventDefault();
    const email = $("register-email").value.trim();
    const password = $("register-password").value;
    if (!email) return show("Enter your email address.");
    if (password.length < 8) return show("Use a password of at least 8 characters.");
    if (password !== $("register-confirm").value) return show("The two passwords do not match.");
    submit(forms.register, "/api/register", { email, password });
  });
})();
