# twich_tip_allert_obs_plugin

TWICH Tip Alert – OBS Plugin

Powered by EddieLives_bot on Telegram
Official TWICHCOIN tipping & faucet service

✨ What is this?

  TWICH Tip Alert is an OBS Studio plugin that displays real-time on-stream alerts for TWICHCOIN (SUI token) tips received via Telegram.
  It listens to messages from EddieLives_bot, the official TWICHCOIN tipping and faucet bot, and renders:

  🎥 Tier-based WebM animations
  📝 Fully customizable text overlays
  🎨 Configurable fonts, colors, position, fades
  🔐 Secure Telegram login via your own API credentials
  
    No browser sources.
    No third-party services.
    Everything runs locally inside OBS.

🧠 How it works (high level)

  You install the OBS plugin
  You log in to Telegram inside OBS (via TDLib)
  The plugin listens to EddieLives_bot
  When a TWICHCOIN tip arrives:
  The correct animation tier is selected
  Text is rendered using your template
  Fade-in / fade-out is applied
  Everything is drawn directly in OBS

📦 Installation
  Requirements
  
    OBS Studio 64-bit
    Windows 10 / 11
    Telegram account
    Telegram API credentials (free)

Install steps"

  Download the latest installer from the Releases page
  👉 GitHub → Releases → TWICH_Tip_Alert_Setup.exe
  
  Close OBS Studio (important)
  Run the installer
  It automatically installs into:
  obs-studio\obs-plugins\64bit

  Required DLLs are included (OpenSSL, zlib, TDLib)

Launch OBS Studio

Add a new source:

Sources → + → TWICH Tip Alerts (Telegram)

🔐 Telegram setup (required)

  This plugin uses your own Telegram session, not a shared service.
  
  Step 1: Get Telegram API credentials
  
    Open 👉 https://my.telegram.org
    Log in with your phone number
    Click “API development tools”
    Copy  api_id &  api_hash

Step 2: Log in inside OBS

In the plugin properties:

  Open Advanced
  
  Enter API ID & API HASH
  Click Save credentials
  
  Follow the login flow:
  Enter phone
  Enter code
  
  Enter 2FA password (if enabled)
  
  When ready, status will show READY (logged in)

🎥 Alert media (tiered)

You can configure 3 alert tiers based on the tip amount.

  Default thresholds Tier 1 → 0, Tier 2 → 10, Tier 3 → 50

  Each tier can play its own WebM animation.

Logic:

  The highest tier whose threshold is met is played.
  
  📝 Text overlay features
  
  Fully customizable per alert:
  
  Text template
  {user} tipped {amount} {symbol}
  {message}
  
  
  Available variables:
  
    {user}
    {amount}
    {symbol}
    {message}
  
  Text styling
  
    Color picker
    Font family (Arial / Segoe UI / Roboto or any installed font)
    Font size
    Optional outline
    Outline thickness
  
  ⚠️ If you select a font not installed on your system, nothing is rendered
  (this is intentional – no fallback).
  
  Position & animation
  
  Position preset:
  
    Top, Center, Bottom
    Margin control
    Smooth fade-in / fade-out
    Independent from media animation

🧪 Test alert

  There is a Test Alert button in the properties panel to instantly trigger a fake tip and preview your setup.

🧹 Uninstalling

  Go to Windows → Add or Remove Programs
  Find TWICH Tip Alert
  Uninstall
  OBS files are removed cleanly.

🛡 Security notes

  Your Telegram session is stored locally
  API credentials are never sent anywhere
  No cloud services
  No browser embeds
  No third-party APIs

🪙 About TWICHCOIN

TWICHCOIN is a token on the SUI blockchain designed for streaming and tipping.

EddieLives_bot is the official TWICHCOIN tipping & faucet service on Telegram.

This plugin is purpose-built for that ecosystem.


❤️ Credits
  OBS Studio
  Telegram TDLib
  TWICHCOIN project
  
