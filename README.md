# TWICH Tip Alert – OBS Plugin

Real-time TWICHCOIN tipping alerts for OBS Studio, powered by Telegram's EddieLives_bot.

![Demo](https://img.shields.io/badge/status-active-success) ![Platform](https://img.shields.io/badge/platform-Windows-blue) ![OBS](https://img.shields.io/badge/OBS-64--bit-6441a5)

## ✨ Overview

TWICH Tip Alert is an OBS Studio plugin that displays real-time on-stream alerts for TWICHCOIN (powered by SUI blockchain) tips received via Telegram. TWICHCOIN is the official tipping token for streamers ([twichcoin.org](https://twichcoin.org)).

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
1. **Download** the latest installer from [Releases](https://github.com/TwichCoin/twich_tip_allert_obs_plugin/releases)
2. **Close OBS Studio** (important!)
3. **Run** `TWICH_Tip_Alert_Setup.exe`
4. The installer will automatically place files in `obs-studio\obs-plugins\64bit`
5. Launch OBS Studio
6. Add a new source: **Sources → + → TWICH Tip Alerts (Telegram)**

## 🔐 Complete Setup Guide

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

### Step 3: Register with EddieLives_bot & Set Up Wallet
**Before you can receive tips, you must register with EddieLives_bot:**

1. **Set up a SUI wallet** (if you don't have one):
   - Recommended: [Slush Wallet](https://slush.app) - Official SUI wallet
   - Alternative: [Ethos Wallet](https://ethoswallet.xyz/)

2. **Register with EddieLives_bot:**
   - Open Telegram and find **@EddieLives_bot**
   - Start a conversation and use `/start`
   - Follow the bot's instructions to:
     - Register your Telegram ID
     - Connect your SUI wallet address
     - Verify your registration

3. **Share your Telegram ID with your community:**
   - Display your Telegram ID in your stream overlay
   - Add it to your social media profiles
   - Include it in your stream title/description
   - Example: "Tip me via @YourTelegramID on EddieLives_bot!"

**Important:** The plugin will only show alerts for tips sent to your registered Telegram account via EddieLives_bot.

## 💰 Tipping System Explained

### How It Works:
1. **Offchain Tipping (Instant & Free):**
   - Viewers tip you using EddieLives_bot
   - Tips are recorded offchain (no gas fees)
   - You receive instant Telegram notifications
   - OBS plugin displays the alert

2. **Onchain Conversion:**
   - Accumulated offchain tips are stored in the bot
   - When ready, use `/claim` command to transfer to your SUI wallet
   - Conversion happens onchain (small gas fee applies - you pay the gas, ~2€ worth of SUI covers a LOT of transactions)
   - You receive actual TWICHCOIN tokens in your wallet

### For Your Viewers:
Share this simple guide with your audience:
```text
How to Tip:
1. Open Telegram, PM @EddieLives_bot with /start or /menu commands
2. Register with or without wallet (wallet needed for withdrawals)
3. Use /drop for airdrop to get some TWICHCOIN tokens to start
4. Tip me with: /tip @StreamerID amount message
5. Let's have fun!
````

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

## 🧪 Testing

Use the **Test Alert** button in the plugin properties to instantly trigger a fake tip and preview your setup.

## 🧹 Uninstallation

1. Go to **Windows → Add or Remove Programs**
2. Find **TWICH Tip Alert**
3. Click **Uninstall**
4. All OBS plugin files will be removed cleanly

## 🛡 Security Notes

- **Local Storage:** Your Telegram session is stored locally on your computer only
- **API Security:** Telegram API credentials are never transmitted externally or shared
- **No Third Parties:** No cloud services, external APIs, or browser embeds used
- **Direct Integration:** Everything runs locally within OBS Studio
- **Read-Only Access:** Plugin only **listens** to EddieLives_bot messages
- **No Data Sent:** Does not send any data to EddieLives_bot or anyone else
- **Wallet Safety:** EddieLives_bot will never ask for:
  - Private keys or seed phrases
  - Sensitive personal information
- **Minimal Data:** Only reads incoming tip notifications (no personal data transmitted)
- **Telegram Security:** Standard Telegram 2FA protects your account
- **Transparency:** Codebase is open for security review

## 🪙 About TWICHCOIN & EddieLives_bot

TWICHCOIN is a token on the SUI blockchain designed for streaming and tipping. **EddieLives_bot** is the official TWICHCOIN tipping & faucet service on Telegram that enables:

- **Offchain tipping** (instant, no gas fees)
- **Onchain conversion** of accumulated tips
- **Faucet access** for everyone with 24h cooldown and limited supply.

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

**Need Help?**
- **Plugin issues:** Open a GitHub issue
- **Tipping service:** Contact @EddieLives_bot on Telegram
- **Wallet setup:** Visit [SUI Wallet Documentation](https://docs.sui.io/learn/wallet-browser)

**🎯 Streamer Ready Checklist:**
- [ ] Installed OBS plugin
- [ ] Configured Telegram API credentials
- [ ] Logged in via plugin
- [ ] Created SUI wallet
- [ ] Registered with EddieLives_bot
- [ ] Customized alert appearance
- [ ] Tested with preview button
- [ ] Shared Telegram ID with community
- [ ] Added tipping instructions to stream overlay

**💡 Pro Tip:** Create a !tip command in your chat bot that explains how viewers can tip you via EddieLives_bot!
