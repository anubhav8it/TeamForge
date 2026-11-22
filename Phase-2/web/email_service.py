"""Transactional email for TeamForge (password reset, email verification, decisions, invitations).

One small interface, `EmailService.send(message)`, and a provider behind it chosen by
EMAIL_PROVIDER, so the provider can change without touching any feature:

* "brevo"  - Brevo's transactional email API (https://api.brevo.com/v3/smtp/email). Free plan:
             300 emails a day; needs an account, an API key and a verified sender address (a
             verified domain improves delivery). Standard library only (urllib).
* "outbox" - development only: each email is written as an .html file to EMAIL_OUTBOX_DIR
             (default web/outbox/, not committed) so flows can be tested without a provider.
* unset    - email is off: nothing is sent and the server logs that once per kind of email.

Configuration (environment variables, set in the host's settings, never in the repository):
    EMAIL_PROVIDER   brevo | outbox
    EMAIL_API_KEY    the provider's API key (secret)
    EMAIL_FROM       sender address, e.g. teamforge@example.com (verified with the provider)
    EMAIL_FROM_NAME  sender name (default "TeamForge")
    APP_BASE_URL     public address of the site, e.g. https://teamforge-geu.onrender.com; links in
                     emails are built from it, never from a request's Host header

Sending happens on a background thread, so a slow provider never delays a request. Failures are
logged without the recipient's address or the email's content, and are never reported to the
person as delivered.
"""
from __future__ import annotations

import json
import logging
import os
import re
import secrets
import threading
import time
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

log = logging.getLogger("teamforge.email")


@dataclass
class EmailMessage:
    to: str
    subject: str
    html: str
    text: str
    kind: str = "email"  # for logs and tests: "password_reset", "verify_email" ...


class _NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):  # the API key never follows a redirect
        return None


class EmailProvider:
    name = "none"

    def deliver(self, message: EmailMessage, sender: str, sender_name: str) -> None:
        raise NotImplementedError


class BrevoProvider(EmailProvider):
    name = "brevo"
    URL = "https://api.brevo.com/v3/smtp/email"

    def __init__(self, api_key: str):
        self.api_key = api_key
        self.opener = urllib.request.build_opener(_NoRedirect)

    def deliver(self, message, sender, sender_name):
        body = json.dumps({
            "sender": {"email": sender, "name": sender_name},
            "to": [{"email": message.to}],
            "subject": message.subject,
            "htmlContent": message.html,
            "textContent": message.text,
            "tags": [message.kind],
        }).encode()
        request = urllib.request.Request(self.URL, data=body, method="POST", headers={
            "api-key": self.api_key, "Content-Type": "application/json", "Accept": "application/json"})
        try:
            with self.opener.open(request, timeout=20) as response:
                if response.status not in (200, 201, 202):
                    raise RuntimeError(f"Brevo answered {response.status}")
        except urllib.error.HTTPError as error:
            raise RuntimeError(f"Brevo answered {error.code}") from None


class OutboxProvider(EmailProvider):
    """Development only: writes each email to a file instead of sending it."""
    name = "outbox"

    def __init__(self, folder: Path):
        self.folder = folder

    def deliver(self, message, sender, sender_name):
        self.folder.mkdir(parents=True, exist_ok=True)
        stamp = time.strftime("%Y%m%d-%H%M%S")
        safe = re.sub(r"[^a-z0-9]+", "-", message.to.lower())[:60]
        path = self.folder / f"{stamp}-{message.kind}-{safe}-{secrets.token_hex(3)}.html"
        path.write_text(f"<!-- To: {message.to} | From: {sender_name} <{sender}> | Subject: {message.subject} -->\n"
                        + message.html, encoding="utf-8")


class EmailService:
    def __init__(self, provider: EmailProvider | None, sender: str, sender_name: str, base_url: str):
        self.provider = provider
        self.sender = sender
        self.sender_name = sender_name or "TeamForge"
        self.base_url = base_url.rstrip("/")
        self.pool = ThreadPoolExecutor(max_workers=2, thread_name_prefix="email")
        self._warned: set[str] = set()
        self.lock = threading.Lock()

    @classmethod
    def from_environment(cls, web_dir: Path) -> "EmailService":
        kind = os.environ.get("EMAIL_PROVIDER", "").strip().lower()
        sender = os.environ.get("EMAIL_FROM", "").strip()
        base_url = os.environ.get("APP_BASE_URL", "").strip()
        provider: EmailProvider | None = None
        if kind == "brevo":
            key = os.environ.get("EMAIL_API_KEY", "").strip()
            if key and sender:
                provider = BrevoProvider(key)
            else:
                log.warning("EMAIL_PROVIDER=brevo needs EMAIL_API_KEY and EMAIL_FROM: email is off.")
        elif kind == "outbox":
            provider = OutboxProvider(Path(os.environ.get("EMAIL_OUTBOX_DIR", web_dir / "outbox")))
            sender = sender or "teamforge@localhost"
        elif kind:
            log.warning("Unknown EMAIL_PROVIDER %r: email is off.", kind)
        return cls(provider, sender, os.environ.get("EMAIL_FROM_NAME", "TeamForge").strip(), base_url)

    @property
    def enabled(self) -> bool:
        return self.provider is not None

    @property
    def links_ready(self) -> bool:
        """Emails with links need APP_BASE_URL (links are never built from request headers)."""
        return bool(self.base_url)

    def link(self, path: str) -> str:
        return self.base_url + path

    def send(self, message: EmailMessage) -> bool:
        """Queues the email. True only means it was handed to a provider, not that it arrived."""
        if not self.enabled:
            with self.lock:
                if message.kind not in self._warned:
                    self._warned.add(message.kind)
                    log.info("email is not set up (EMAIL_PROVIDER): %s emails are not sent", message.kind)
            return False
        self.pool.submit(self._deliver, message)
        return True

    def _deliver(self, message: EmailMessage) -> None:
        try:
            self.provider.deliver(message, self.sender, self.sender_name)
            log.info("email handed to %s: %s", self.provider.name, message.kind)
        except Exception as error:  # never the address or the content in the log
            log.warning("email via %s failed (%s): %s", self.provider.name, message.kind, type(error).__name__ + ": " + str(error)[:120])
