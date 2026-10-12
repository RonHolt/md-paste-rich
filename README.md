# md-paste-rich

Paste markdown as rich text on Hyprland. Copy markdown (Claude Code's `/copy`, a README,
notes), press Ctrl+Shift+V in ClickUp, Gmail, Google Docs or Slack, and it lands with real
bold, italics, links, lists, headings and code instead of `**asterisks**`.

It replaces the round-trip of pasting into a Google Doc with a "paste from markdown"
extension and copying the rich text back out.

## What happens where

| Focused app | Ctrl+Shift+V does |
|---|---|
| ClickUp, Gmail, Google Docs, most web editors | Pastes formatted HTML |
| Slack | Pastes Slack markup (`*bold*`, `_italic_`, `•` bullets); Slack renders it on send |
| Terminals | Passes Ctrl+Shift+V through, so terminal paste works as before |
| Obsidian | Passes through (markdown is native there) |
| Anything, clipboard holds web content (not markdown) | Passes through, keeping the browser's "paste as plain text" |

Ctrl+V is never touched. After a rich paste the clipboard keeps both versions: rich-text
apps get the HTML, terminals and code editors get the original markdown.

## Requirements

- Hyprland 0.56+ with a Lua config (keys are sent with `hl.dsp.send_key_state`). Developed
  on [Omarchy](https://omarchy.org/) 4, which is not required.
- A compositor with `ext-data-control-v1` (Hyprland has it).
- `cmark-gfm`, `wl-clipboard` (`wl-paste`), `jq`, `python3`, and optionally `libnotify`.
- To build `wl-copy-multi`: a C compiler, `wayland` (`wayland-scanner`, `wayland-client`),
  `wayland-protocols`.

On Arch:

```bash
sudo pacman -S --needed cmark-gfm wl-clipboard jq python libnotify base-devel wayland wayland-protocols
```

## Install

```bash
git clone https://github.com/RonHolt/md-paste-rich.git
cd md-paste-rich
make install PREFIX=~/.local     # or: sudo make install (PREFIX=/usr/local)
```

That installs three commands: `md-paste-rich`, `md-to-slack` and `wl-copy-multi`. Make
sure the install directory is on `PATH` for Hyprland (`~/.local/bin` is on Omarchy).

## Key binding

Add to your Hyprland config. Plain Hyprland Lua:

```lua
hl.bind("CTRL + SHIFT + V", hl.dsp.exec_cmd("md-paste-rich --ctrl-shift-v"),
  { description = "Paste markdown as rich text" })
```

Omarchy (`~/.config/hypr/bindings.lua`), optionally with Super+Shift+V next to Omarchy's
Super+V universal paste:

```lua
o.bind("CTRL + SHIFT + V", "Paste markdown as rich text", "md-paste-rich --ctrl-shift-v")
o.bind("SUPER + SHIFT + V", "Paste markdown as rich text", "md-paste-rich")
```

Check with `hyprctl reload && hyprctl configerrors`. Taking over Ctrl+Shift+V globally is
safe because terminals get it passed straight back. To undo, delete the binding.

## Usage

```
md-paste-rich                 convert the clipboard and paste with Ctrl+V
md-paste-rich --ctrl-shift-v  same, but pass Ctrl+Shift+V through when there is nothing
                              to convert (use this for a Ctrl+Shift+V binding)
md-paste-rich --copy          convert only; paste yourself
```

## Configuration

Optional, `~/.config/md-paste-rich/config`, sourced as shell. Defaults:

```bash
# Window classes (hyprctl activewindow) that get the key passed through.
TERMINAL_CLASSES='com.mitchellh.ghostty Alacritty kitty foot footclient org.wezfurlong.wezterm
  org.kde.konsole org.gnome.Console org.gnome.Terminal com.gexperts.Tilix XTerm URxvt st-256color'
PASSTHROUGH_CLASSES='md.obsidian.Obsidian'
# Apps that only read plain text on paste; they get Slack markup.
SLACK_CLASSES='slack Slack'
# Empty line between paragraphs/blocks (rich editors drop paragraph margins on paste).
BLANK_LINES=1
```

Windows carrying Omarchy's `terminal` tag are always treated as terminals.

## How it works

1. Reads the clipboard text and strips the indent shared by every line (terminal copies
   carry a left margin that would otherwise turn everything into a code block).
2. Converts with `cmark-gfm` (GitHub-flavored: tables, strikethrough, autolinks, task
   lists, hard line breaks), then puts an empty paragraph between top-level blocks.
3. Puts the HTML as `text/html` and the original markdown as `text/plain` on the clipboard
   with `wl-copy-multi`.
4. Sends Ctrl+V to the focused window with `hl.dsp.send_key_state`.

### wl-copy-multi

`wl-copy` serves one payload under every MIME type it offers: copy HTML with
`--type text/html` and it also answers `text/plain` requests with the same tags, so any
plain-text reader gets raw HTML. `wl-copy-multi` is a small `ext-data-control-v1` client
that offers each type with its own content, and like `wl-copy` forks and serves until
another client takes the clipboard. It is usable on its own:

```bash
wl-copy-multi text/html=page.html text/plain=page.md   # FILE may be - for stdin
```

`text/plain` also answers `text/plain;charset=utf-8`, `UTF8_STRING`, `STRING` and `TEXT`.

### Slack

The Slack desktop app never formats a paste: given HTML and plain text it reads the plain
text, and given HTML alone it pastes nothing. It does render its own markup when a
message is sent, so for Slack `md-to-slack` converts the markdown (via the cmark-gfm XML
tree) to that markup, it is pasted, and a second later the HTML + markdown pair is put
back. Headings become bold lines, links `text (url)` (Slack's `<url|text>` form only works
for bots), tables an aligned code block, task items ☐ / ☑.

## Limitations

- Hyprland only, for sending the paste keystroke. Conversion and `wl-copy-multi` are
  compositor-agnostic.
- Tested with ClickUp, Google Docs and Slack in Brave / the Slack desktop app on
  Hyprland 0.56. Other editors may treat the HTML differently.
- Raw HTML in the markdown is passed through as HTML, except tags GitHub also filters
  (`<script>`, `<iframe>`, `<style>`, ...), which come out as text.

## Privacy

Everything runs locally; nothing is sent anywhere. Clipboard content a password manager
marks sensitive (`x-kde-passwordManagerHint`) is never converted or re-copied, so it
doesn't end up in clipboard history.

## License

MIT
