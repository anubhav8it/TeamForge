"use strict";
// Small interactive touches for the TeamForge web pages, all optional and all off for people who
// ask for reduced motion: headlines that rise in word by word ([data-reveal]), cards that tilt
// towards the pointer ([data-tilt]), a panel edge that lights up under the pointer
// ([data-spotlight]), and a ten-word quote that changes now and then (#quote). On the app page
// it also stops the animated background once the app has started (#loading gets "hidden").
(function () {
  const reduceMotion = window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  const finePointer = window.matchMedia && window.matchMedia("(pointer: fine)").matches;

  // Headlines: each word becomes a span that rises in after the previous one.
  for (const el of document.querySelectorAll("[data-reveal]")) {
    const words = el.textContent.trim().split(/\s+/);
    el.textContent = "";
    el.setAttribute("aria-label", words.join(" "));
    words.forEach((word, i) => {
      const span = document.createElement("span");
      span.className = "word";
      span.setAttribute("aria-hidden", "true");
      span.textContent = word;
      span.style.animationDelay = (120 + i * 90) + "ms";
      el.appendChild(span);
      if (i < words.length - 1) el.appendChild(document.createTextNode(" "));
    });
    el.classList.add("revealing");
  }

  if (!reduceMotion && finePointer) {
    // Cards tilt a few degrees towards the pointer and settle back when it leaves.
    for (const card of document.querySelectorAll("[data-tilt]")) {
      card.addEventListener("pointermove", e => {
        const r = card.getBoundingClientRect();
        const x = (e.clientX - r.left) / r.width - 0.5;
        const y = (e.clientY - r.top) / r.height - 0.5;
        card.style.transform = "perspective(700px) rotateX(" + (-y * 6).toFixed(2) + "deg) rotateY(" + (x * 8).toFixed(2) + "deg) translateY(-2px)";
      });
      card.addEventListener("pointerleave", () => { card.style.transform = ""; });
    }
  }
  // The edge light follows the pointer (a CSS variable feeds a radial gradient).
  for (const panel of document.querySelectorAll("[data-spotlight]")) {
    panel.addEventListener("pointermove", e => {
      const r = panel.getBoundingClientRect();
      panel.style.setProperty("--mx", ((e.clientX - r.left) / r.width * 100).toFixed(1) + "%");
      panel.style.setProperty("--my", ((e.clientY - r.top) / r.height * 100).toFixed(1) + "%");
    });
  }

  // Ten-word quotes from the app's library (core/Motivation.cpp), the collaboration and growth ones.
  const quotes = [
    ["Great teams transform individual strengths into", "shared", "achievements every day."],
    ["Your next great opportunity often begins with the right", "collaboration", "."],
    ["Bring your skills, share your ideas and build something", "meaningful", "."],
    ["Complementary skills make teams that are greater than their", "parts", "."],
    ["Great skills become powerful when the right team comes", "together", "."],
    ["Progress beats perfection; today's profile is a strong starting", "point", "."],
  ];
  const quote = document.getElementById("quote");
  if (quote) {
    let index = Math.floor(Math.random() * quotes.length);
    const render = () => {
      const [before, key, after] = quotes[index];
      quote.textContent = "";
      quote.append(before + " ");
      const em = document.createElement("em");
      em.textContent = key;
      quote.append(em, after.startsWith(".") ? after : " " + after);
    };
    render();
    if (!reduceMotion) {
      setInterval(() => {
        if (document.hidden) return;
        quote.classList.add("swap");
        setTimeout(() => {
          index = (index + 1) % quotes.length;
          render();
          quote.classList.remove("swap");
        }, 450);
      }, 8000);
    }
  }

  // App page: once the loading overlay is hidden the app is running; stop the background.
  const loading = document.getElementById("loading");
  if (loading && window.MutationObserver) {
    const observer = new MutationObserver(() => {
      if (loading.classList.contains("hidden") && window.TFBackdrop) {
        window.TFBackdrop.stop();
        observer.disconnect();
      }
    });
    observer.observe(loading, { attributes: true, attributeFilter: ["class"] });
  }
})();
