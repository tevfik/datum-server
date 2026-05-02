# datumctl

Command-line interface for the Datum IoT platform.

## Install

```bash
go install ./cmd/datumctl
# or
make build
```

## Modes

datumctl ships two complementary surfaces:

1. **Interactive** (recommended for first-time users)

   ```bash
   datumctl interactive   # or: datumctl i
   ```

   The interactive menu is **auto-derived from the command tree**, so any new
   subcommand or flag becomes available there with no extra wiring. Each
   leaf prompts for its positional arguments, then offers a multi-select
   picker for any flags you want to override before running the command.

2. **Direct verbs** (kubectl-style — scripts, CI, power users)

   ```bash
   datumctl <noun> <verb> [flags]
   ```

## Common verbs

| Noun         | Verbs                                                                   |
|--------------|-------------------------------------------------------------------------|
| `auth`       | `me`, `sessions`, `providers`, `refresh`, `change-password`, `forgot-password`, `reset-password`, `delete-account`, `push-token {list,add,delete}` |
| `login`      | top-level shortcut for `/auth/login`                                    |
| `logout`     | clear local credentials + revoke server session                         |
| `register`   | `/auth/register`                                                        |
| `setup`      | `/sys/setup` first-time bootstrap (creates admin user)                  |
| `device`     | `list`, `get`, `create`, `update`, `delete`, `rotate-key`, `revoke-key`, `token-info`, `update-firmware` |
| `data`       | `get`, `post`, `stats`                                                  |
| `command`    | `send`, `list`, `get`, `history`, `cancel`                              |
| `bucket`     | `list`, `create`, `delete`, `put`, `get`, `presign`                      |
| `key`        | `list`, `create`, `delete`                                              |
| `mqtt`       | `stats`, `clients`, `publish`                                           |
| `notify`     | `publish <topic>`, `subscribe <topic>` (json/sse/raw)                   |
| `rules`      | `list`, `get`, `create`, `delete`, `enable`, `disable` (admin)          |
| `sys`        | `info`, `time`, `ip`, `status`, `metrics`                               |
| `admin`      | `list-users`, `create-user`, `update-user`, `delete-user`, `reset-password`, `stats`, `get-config`, `toggle-registration`, `reset-system` |
| `provision`  | `register`, `list`, `status`, `cancel`                                  |
| `logs`       | view recent logs (`-f` follow, `-l` level filter, `-n` lines)           |
| `status`     | `/health`                                                               |
| `version`    | print build info                                                        |
| `completion` | `bash`, `zsh`, `fish`, `powershell`                                     |

`datumctl --help` lists every command. `datumctl <command> --help` shows
its flags.

## Global flags

| Flag           | Default                | Notes                              |
|----------------|------------------------|------------------------------------|
| `--server`     | `http://localhost:8000`| Datum server base URL              |
| `--token`      |                        | JWT for the request                |
| `--api-key`    |                        | Stable user/device API key         |
| `--config`     | `~/.datumctl.yaml`     | Override config path               |
| `--json`       | `false`                | Emit raw JSON instead of pretty    |
| `--curl`       | `false`                | Print equivalent `curl` command    |
| `--insecure`   | `false`                | Skip TLS verification              |
| `-v, --verbose`| `false`                | Verbose request logging            |

## Shell completion

Cobra's auto-completion is enabled. Generate a snippet for your shell:

```bash
# bash
source <(datumctl completion bash)
# add to ~/.bashrc:
echo 'source <(datumctl completion bash)' >> ~/.bashrc

# zsh
datumctl completion zsh > "${fpath[1]}/_datumctl"

# fish
datumctl completion fish | source

# powershell
datumctl completion powershell > datumctl.ps1
```

## Bucket recipes

```bash
datumctl bucket create images
datumctl bucket put images cats/1.jpg ./local.jpg --content-type image/jpeg
datumctl bucket presign images cats/1.jpg --method GET --expires 600
datumctl bucket list images           # list objects
datumctl bucket delete images cats/1.jpg
```

## Notification recipes

```bash
# publish to a topic (ntfy-protocol compatible)
datumctl notify publish alerts --message "Pump #2 offline" --priority high --tags "warning,pump"

# stream new messages (json | sse | raw)
datumctl notify subscribe alerts --format json
echo "raw text via stdin" | datumctl notify publish alerts
```

## Rules engine (admin)

```bash
datumctl rules list
datumctl rules create --file rule.json
datumctl rules disable rule_abc123
datumctl rules enable  rule_abc123
datumctl rules delete  rule_abc123
```

## Auth recipes

```bash
datumctl auth me
datumctl auth sessions                          # list active JWTs
datumctl auth providers                         # configured OAuth providers
datumctl auth change-password --old-password ... --new-password ...
datumctl auth push-token list
datumctl auth push-token add --platform fcm --token <fcm-token>
datumctl auth push-token delete <token-id>
```

## Version

```bash
datumctl version
datumctl version --json
```
