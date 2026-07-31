# GIT :: Quick Guide

## Autenticazione su GitHub

Al primo push GitHub chiede utente e password: come password NON va la password dell'account ma un Personal Access Token (Settings > Developer settings > Personal access tokens su GitHub).

## Ignore files

Crea un file chiamato `.gitignore` e mettici quello che vuoi NON venga versionato:

```
# .gitignore
.venv/
__pycache__/
*.pyc
.env
```

## Create local repo

```
cd \path\to\project
git init             # inizializza il repo
git add .
git commit -m "first commit"
```

## Undo `add .`

```
git reset
```

## Pubblica repo su github

Prima crea repo VUOTO su github, poi

```
git remote add origin https://github.com/tuo-utente/nome-repo.git
git push -u origin main
```

## Clone

Clone di default crea una cartella col nome del repo

```
git clone https://github.com/tuo-utente/nome-repo.git
cd nome-repo
```

Se la cartella c'è già (ed è vuota):

```
cd nome-repo
git clone https://github.com/tuo-utente/nome-repo.git .
```

## Logs

```
git status          # cosa è cambiato? cosa è in staging?
git diff            # mostra le modifiche riga per riga
git log --oneline   # storia dei commit, compatta
```

## Commit

```
git add .                        # aggiunge tutto al prossimo commit
git commit -m "descrizione"      # crea il commit
git push                         # manda su GitHub
```

## Pull

```
git pull
```

## Pull specific tag

```
git fetch --tags
git checkout v0.1.0
```

## Tag version

```
git tag v0.1.0
git push origin v0.1.0
```

## Git slang

| Emoji | Tipo | Quando |
|---|---|---|
| ✨ | feat | nuova funzionalità |
| 🐛 | fix | bug fix |
| 📝 | docs | documentazione |
| ♻️ | refactor | refactoring |
| 🚀 | chore | deploy / bump versione |
| 🔧 | chore | configurazione |
| ➕ | chore | aggiunge dipendenza |

## Oops I committed file.ext

Prima aggiungi il file al .gitignore, poi:

```
git rm --cached file.ext
git add .gitignore
git commit -m "chore: rimuove file.ext dal tracking, aggiorna gitignore"
git push
```

## Tag delete

```
git tag -d v0.1.0                  # cancella in locale
git push origin --delete v0.1.0    # cancella su GitHub
```