# Moving Datum Camera App to a Standalone Repository

This guide explains how to extract the `examples/datum_camera_app` directory into its own git repository while preserving its commit history. This is recommended if you plan to develop the mobile app independently from the backend server.

## Prerequisites

- `git` installed.
- [`git-filter-repo`](https://github.com/newren/git-filter-repo) installed (Python script, install via `pip install git-filter-repo` or package manager).
  - *Note: `git filter-branch` is deprecated and slow; `git filter-repo` is the modern standard.*

## Migration Steps

### 1. Prepare a Fresh Clone
Always perform this operation on a fresh clone to avoid messing up your current working directory.

```bash
# Clone the repository to a temporary location
git clone https://git.bezg.in/batterymanager/datum-server.git datum-mobile-migration
cd datum-mobile-migration
```

### 2. Filter the Repository
This command rewrites the git history to keep **only** the contents of `examples/datum_camera_app` and moves them to the root of the new repository.

```bash
# Filter to keep only the mobile app folder and move its contents to root
git filter-repo --path examples/datum_camera_app/ --to-subdirectory-filter /
```

*Explanation:*
- `--path examples/datum_camera_app/`: Keeps only files in this folder.
- `--to-subdirectory-filter /`: Moves the contents of that folder to the root (so `examples/datum_camera_app/pubspec.yaml` becomes `./pubspec.yaml`).

### 3. Verify the Content
Check that the files are now at the root and the history is preserved.

```bash
ls -la
# You should see pubspec.yaml, lib/, android/, etc. directly here.

git log --oneline
# You should see commits related to the mobile app.
```

### 4. Push to New Repository
Now connect this local repository to your new remote repository (e.g., on GitHub or Gitea).

```bash
# Initialize new remote
git remote add origin <NEW_REPOSITORY_URL>

# Rename default branch if needed (e.g., master -> main)
git branch -M main

# Push code and tags
git push -u origin main --force
```

## Post-Migration Checklist

1. **Update CI/CD**: If you had CI pipelines (GitHub Actions, etc.) relying on the old path, update them to run at the root of the new repo.
2. **Update README**: You might want to update the `README.md` to remove references to the parent `datum-server` repo structure.
3. **Clean up Old Repo**: In the original `datum-server` repository, you can now delete the `examples/datum_camera_app` folder and add a link to the new repository in the README.

```bash
# In the original datum-server repo:
rm -rf examples/datum_camera_app
git commit -m "refactor: move mobile app to separate repo <LINK>"
```
