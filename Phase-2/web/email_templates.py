"""TeamForge's transactional emails: branded HTML (table layout and inline styles, which email
programs understand) plus a plain-text version of each. Every value put into an email is
escaped; links are built from APP_BASE_URL by EmailService.link()."""
from __future__ import annotations

from html import escape

from email_service import EmailMessage

NAVY = "#0D1220"
CARD = "#141B2C"
BORDER = "#232D44"
TEXT = "#F5F7FB"
MUTED = "#8F9BB3"
CORAL = "#F0434A"


def _layout(base_url: str, preheader: str, title: str, paragraphs: list[str], button: tuple[str, str] | None,
            notice: str) -> str:
    logo = f"{base_url}/static/img/teamforge_logo_dark.png" if base_url else ""
    body = "".join(f'<p style="margin:0 0 14px;font-size:15px;line-height:1.6;color:{TEXT};">{p}</p>' for p in paragraphs)
    action = ""
    if button:
        label, href = button
        action = (f'<table role="presentation" cellpadding="0" cellspacing="0" style="margin:22px 0 8px;"><tr>'
                  f'<td style="border-radius:10px;background:{CORAL};background-image:linear-gradient(90deg,{CORAL},#FF7A59);">'
                  f'<a href="{escape(href, quote=True)}" style="display:inline-block;padding:13px 26px;font-size:15px;'
                  f'font-weight:700;color:#FFFFFF;text-decoration:none;border-radius:10px;">{escape(label)}</a></td></tr></table>'
                  f'<p style="margin:10px 0 0;font-size:12px;line-height:1.5;color:{MUTED};">If the button does not work, '
                  f'copy this link into your browser:<br><span style="color:#C9D1E0;word-break:break-all;">{escape(href)}</span></p>')
    header = (f'<img src="{escape(logo, quote=True)}" alt="TeamForge" width="150" style="display:block;border:0;">'
              if logo else f'<span style="font-size:20px;font-weight:800;color:{TEXT};letter-spacing:2px;">TEAM'
                           f'<span style="color:{CORAL};">FORGE</span></span>')
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="dark light"><title>{escape(title)}</title></head>
<body style="margin:0;padding:0;background:{NAVY};font-family:Manrope,Segoe UI,Helvetica,Arial,sans-serif;">
<span style="display:none;max-height:0;overflow:hidden;opacity:0;">{escape(preheader)}</span>
<table role="presentation" width="100%" cellpadding="0" cellspacing="0" style="background:{NAVY};padding:28px 12px;">
<tr><td align="center">
  <table role="presentation" width="100%" cellpadding="0" cellspacing="0" style="max-width:560px;">
    <tr><td style="padding:0 6px 18px;">{header}</td></tr>
    <tr><td style="background:{CARD};border:1px solid {BORDER};border-radius:18px;padding:28px 26px;">
      <div style="height:3px;width:64px;border-radius:2px;background:{CORAL};margin-bottom:18px;"></div>
      <h1 style="margin:0 0 16px;font-size:22px;line-height:1.3;color:{TEXT};">{escape(title)}</h1>
      {body}{action}
    </td></tr>
    <tr><td style="padding:16px 8px 0;font-size:12px;line-height:1.6;color:{MUTED};">{notice}<br>
      TeamForge · Graphic Era (Deemed to be University) · Team DSCPP-III-2026-T238</td></tr>
  </table>
</td></tr></table></body></html>"""


def _text(title: str, lines: list[str], link: str | None, notice: str) -> str:
    out = [title, "", *lines]
    if link:
        out += ["", link]
    return "\n".join(out + ["", notice, "TeamForge"])


def password_reset(to: str, link: str, base_url: str, minutes: int) -> EmailMessage:
    title = "Reset your TeamForge password"
    notice = (f"This link works once and expires in {minutes} minutes. If you did not ask to reset your password, "
              "ignore this email: your password stays as it is. TeamForge never asks for your password by email.")
    paragraphs = ["Someone (we hope you) asked to reset the password of the TeamForge account for this email address.",
                  "Choose a new password with the button below."]
    return EmailMessage(to, title, _layout(base_url, "Choose a new password for TeamForge", title, paragraphs,
                                           ("Choose a new password", link), escape(notice)),
                        _text(title, paragraphs, link, notice), "password_reset")


def verify_email(to: str, link: str, base_url: str, hours: int) -> EmailMessage:
    title = "Confirm your email address"
    notice = (f"This link expires in {hours} hours. If you did not create a TeamForge account, you can ignore this email.")
    paragraphs = ["Welcome to TeamForge! Please confirm that this email address is yours, so password resets and "
                  "updates about your teams reach you."]
    return EmailMessage(to, title, _layout(base_url, "Confirm your email for TeamForge", title, paragraphs,
                                           ("Confirm my email", link), escape(notice)),
                        _text(title, paragraphs, link, notice), "verify_email")


def welcome(to: str, link: str, base_url: str) -> EmailMessage:
    title = "Your TeamForge account is ready"
    notice = "You are receiving this because an account was created with this email address."
    paragraphs = ["Your account is ready. Build your profile, find a team that needs your skills, or start your own.",
                  "Tip: add a profile photo so team leaders recognise you."]
    return EmailMessage(to, title, _layout(base_url, "Welcome to TeamForge", title, paragraphs,
                                           ("Open TeamForge", link), escape(notice)),
                        _text(title, paragraphs, link, notice), "welcome")


def application_decision(to: str, link: str, base_url: str, target: str, kind: str, approved: bool,
                         message: str) -> EmailMessage:
    what = "team" if kind == "team" else "event"
    title = (f"You're in: {target}" if approved else f"Your application to {target}")
    paragraphs = ([f"Good news: your application to the {what} <strong>{escape(target)}</strong> was approved."
                   + (" You can now open the team chat." if kind == "team" else "")]
                  if approved else
                  [f"Your application to the {what} <strong>{escape(target)}</strong> was not accepted this time."])
    if message:
        paragraphs.append(f'Message from the {"team" if kind == "team" else "organiser"}: '
                          f'<em style="color:#C9D1E0;">“{escape(message)}”</em>')
    notice = "You can turn these emails off in TeamForge: Community → Profile → Email updates."
    plain = [p.replace("<strong>", "").replace("</strong>", "") for p in paragraphs[:1]] + ([f"Message: {message}"] if message else [])
    return EmailMessage(to, title, _layout(base_url, title, title, paragraphs, ("Open TeamForge", link), escape(notice)),
                        _text(title, plain, link, notice), "application_decision")


def invitation(to: str, link: str, base_url: str, target: str, kind: str) -> EmailMessage:
    what = "team" if kind == "team" else "event"
    title = f"You're invited to apply to {target}"
    paragraphs = [f"The {what} <strong>{escape(target)}</strong> read your anonymous message and would like your "
                  "application.", "Applying shares your TeamForge profile with them; until then you stay anonymous."]
    notice = "You can turn these emails off in TeamForge: Community → Profile → Email updates."
    plain = [f"The {what} {target} read your anonymous message and would like your application.", paragraphs[1]]
    return EmailMessage(to, title, _layout(base_url, title, title, paragraphs, ("View the invitation", link),
                                           escape(notice)), _text(title, plain, link, notice), "invitation")


def team_notification(to: str, link: str, base_url: str, target: str, text: str) -> EmailMessage:
    title = f"Update from {target}"
    notice = "You can turn these emails off in TeamForge: Community → Profile → Email updates."
    return EmailMessage(to, title, _layout(base_url, title, title, [escape(text)], ("Open TeamForge", link),
                                           escape(notice)), _text(title, [text], link, notice), "team_notification")
