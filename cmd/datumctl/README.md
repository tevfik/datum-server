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

2. **Direct verbs** (kubectl-style — scripts, CI, power users)

   ```bash
   datumctl <noun> <verb> [flags]
   ```

## Common verbs

| Noun       | Verbs                                             |
|------------|---------------------------------------------------|
| `auth`     | `login`, `logout`, `whoami`                       |
| `device`   | `list`, `get`, `create`, `update`, `delete`       |
| `data`     | `push`, `query`, `tail`                           |
| `command`  | `send`, `list`, `get`, `history`, `cancel`        |
| `bucket`   | `list`, `create`, `delete`, `put`, `get`, `presign` |
| `key`      | `list`, `create`, `revoke`                        |
| `mqtt`     | `publish`, `subscribe`, `tail`                    |
| `provision`| `create`, `list`, `revoke`                        |
| `system`   | `health`, `info`                                  |
| `version`  | (top-level) print build info                      |
| `completion` | `bash`, `zsh`, `fish`, `powershell`              |

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

## Version

```bash
datumctl version
datumctl version --json
```
