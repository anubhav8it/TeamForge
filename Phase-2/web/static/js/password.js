"use strict";
// Show / hide for every password field: a button with an eye icon inside the field
// (<button class="reveal" data-reveal-for="input-id">). It is a real button, so it works with the
// keyboard (Tab, Enter, Space) and screen readers hear "Show password" / "Hide password".
(function () {
  const EYE = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M2 12s3.6-7 10-7 10 7 10 7-3.6 7-10 7S2 12 2 12z"/><circle cx="12" cy="12" r="3"/></svg>';
  const EYE_OFF = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M3 3l18 18"/><path d="M10.6 5.1A10.9 10.9 0 0 1 12 5c6.4 0 10 7 10 7a17.6 17.6 0 0 1-3.2 4.2M6.6 6.6C3.8 8.4 2 12 2 12s3.6 7 10 7a10.6 10.6 0 0 0 5.4-1.5"/><path d="M9.9 9.9a3 3 0 0 0 4.2 4.2"/></svg>';
  for (const button of document.querySelectorAll("button.reveal[data-reveal-for]")) {
    const input = document.getElementById(button.dataset.revealFor);
    if (!input) continue;
    const update = () => {
      const shown = input.type === "text";
      button.innerHTML = shown ? EYE_OFF : EYE;
      button.setAttribute("aria-pressed", String(shown));
      button.setAttribute("aria-label", shown ? "Hide password" : "Show password");
      button.title = shown ? "Hide password" : "Show password";
    };
    button.addEventListener("click", event => {
      event.preventDefault();
      const end = input.selectionEnd;
      input.type = input.type === "password" ? "text" : "password";
      update();
      input.focus();
      try { input.setSelectionRange(end, end); } catch (e) { /* not all types allow it */ }
    });
    // Hide again when the form is sent, so a visible password is not left on screen.
    if (input.form) input.form.addEventListener("submit", () => { input.type = "password"; update(); });
    update();
  }
})();
