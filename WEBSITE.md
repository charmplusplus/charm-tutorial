## Building and previewing the tutorial website

The tutorial site is built with MkDocs + Material theme.
Config file: mkdocs.yml  Source files: tutorial/*.md  Output: site/

### Preview locally

    cd /Users/kale/software/tutorialcharmclaude
    mkdocs serve

Opens a live server at http://127.0.0.1:8000
Pages reload automatically when you edit any .md file.

### Build static HTML

    mkdocs build

Writes the complete site to site/  Ready to copy to any web server.

### Deploy to GitHub Pages

    mkdocs gh-deploy

Pushes the built site to the gh-pages branch of the GitHub repo.
The site becomes available at https://<username>.github.io/<reponame>/

### Adding or reordering chapters

Edit the nav: section in mkdocs.yml.
Each line is:  Display Name: filename.md
The order in the file is the order in the sidebar and prev/next buttons.

### Ignoring the startup warning

MkDocs prints a warning about a future version (2.0) that is not yet released.
It does not affect the current build. Safe to ignore.
