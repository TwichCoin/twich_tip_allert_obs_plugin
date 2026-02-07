# TWICH Tip Alert – OBS Plugin

Real-time TWICHCOIN tipping alerts for OBS Studio, powered by Telegram's EddieLives_bot.

![Demo](https://img.shields.io/badge/status-active-success) ![Platform](https://img.shields.io/badge/platform-Windows-blue) ![OBS](https://img.shields.io/badge/OBS-64--bit-6441a5)

## ✨ Overview

TWICH Tip Alert is an OBS Studio plugin that displays real-time on-stream alerts for TWICHCOIN (SUI token) tips received via Telegram. It listens to messages from EddieLives_bot (the official TWICHCOIN tipping and faucet service) and renders customizable alerts directly within OBS.

**Key Features:**
- 🎥 **Tier-based WebM animations** (3 configurable tiers)
- 📝 **Fully customizable text overlays** with dynamic variables
- 🎨 **Complete styling control** – fonts, colors, positioning, fades
- 🔐 **Secure local Telegram login** using your own API credentials
- 🚫 **No browser sources or third-party services** – everything runs locally

## 📦 Installation

### Requirements
- OBS Studio 64-bit
- Windows 10 / 11
- Telegram account
- Telegram API credentials (free)

### Installation Steps
1. **Download** the latest installer from [Releases](https://github.com/yourusername/twitch_tip_alert_obs_plugin/releases)
2. **Close OBS Studio** (important!)
3. **Run** `TWICH_Tip_Alert_Setup.exe`
4. The installer will automatically place files in `obs-studio\obs-plugins\64bit`
5. Launch OBS Studio
6. Add a new source: **Sources → + → TWICH Tip Alerts (Telegram)**

## 🔐 Telegram Setup (Required)

This plugin uses your own Telegram session for security.

### Step 1: Get Telegram API Credentials
1. Visit [https://my.telegram.org](https://my.telegram.org)
2. Log in with your phone number
3. Click "API development tools"
4. Copy your `api_id` & `api_hash`

### Step 2: Log In Within OBS
1. In the plugin properties, open **Advanced** settings
2. Enter your API ID & API HASH
3. Click **Save credentials**
4. Follow the login flow:
   - Enter phone number
   - Enter verification code
   - Enter 2FA password (if enabled)
5. Status will show **READY (logged in)** when successful

## 🎥 Alert Configuration

### Tier-Based Animations
Configure 3 alert tiers based on tip amounts:
- **Default thresholds:** Tier 1 (0+), Tier 2 (10+), Tier 3 (50+)
- Each tier can have its own WebM animation
- The highest tier whose threshold is met will be played

### Text Overlay Customization
**Template variables available:**
- `{user}` – Tipper's username
- `{amount}` – Tip amount
- `{symbol}` – Currency symbol (TWICH)
- `{message}` – Optional message from tipper

**Default template:** `{user} tipped {amount} {symbol}`

**Styling options:**
- Color picker
- Font family (uses installed system fonts)
- Font size
- Optional outline with adjustable thickness

**⚠️ Font Note:** If you select a font not installed on your system, text won't render (no automatic fallback).

### Position & Animation
- **Position presets:** Top, Center, Bottom
- Margin controls
- Smooth fade-in / fade-out transitions
- Independent timing from media animations

## 🧪 Testing

Use the **Test Alert** button in the plugin properties to instantly trigger a fake tip and preview your setup.

## 🧹 Uninstallation

1. Go to **Windows → Add or Remove Programs**
2. Find **TWICH Tip Alert**
3. Click **Uninstall**
4. All OBS plugin files will be removed cleanly

## 🛡 Security Notes

- Your Telegram session is stored locally
- API credentials are never transmitted externally
- No cloud services or third-party APIs
- No browser embeds – everything runs in OBS

## 🪙 About TWICHCOIN

TWICHCOIN is a token on the SUI blockchain designed for streaming and tipping. EddieLives_bot is the official TWICHCOIN tipping & faucet service on Telegram. This plugin is purpose-built for that ecosystem.

## ❤️ Credits

- [OBS Studio](https://obsproject.com/)
- [Telegram TDLib](https://core.telegram.org/tdlib)
- [TWICHCOIN Project](https://twichcoin.org/)
- EddieLives_bot on Telegram

## 📄 License

MIT

## 🤝 Contributing

TwichTwit

---
