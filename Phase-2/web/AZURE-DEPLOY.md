# TeamForge on Azure, at (almost) no cost, with your own domain

This moves the web version from Render to Azure without changing the app:

```
your domain (HTTPS, free managed certificate)
        │
Azure Container App  ── image from ghcr.io (GitHub's registry, free), built by GitHub Actions
  web/server.py           scales to zero when nobody uses it; 0.25 CPU, 0.5 GB
        │ DATABASE_URL (SSL)
Azure Database for PostgreSQL Flexible Server, Burstable B1ms
        │ (optional) AZURE_DOCINTEL_*
Azure AI Document Intelligence F0: reads photos and scans of resumes
```

**What it costs on Azure for Students** (as Microsoft's student offer lists it; check the
"Free services" page of your subscription):

| Part | Allowance |
|---|---|
| Container Apps | always free monthly allowance: 180,000 vCPU-seconds, 360,000 GiB-seconds, 2 million requests. At 0.25 CPU that is about 200 busy hours a month; idle time costs nothing |
| PostgreSQL Flexible Server | free for 12 months: 750 hours a month of B1ms, 32 GB storage, 32 GB backup. **Only that size is free.** The 12 months start when you create it |
| Document Intelligence | F0: free, with F0's limits (4 MB, 2 PDF pages, 20 requests a minute) |
| ghcr.io image | free on GitHub |
| Your domain | the domain itself (some are free for a year with the GitHub Student Developer Pack); the HTTPS certificate is free |

Nothing here can charge a card: Azure for Students has a spending limit. If the $100 credit ran
out, Azure would email you and disable the resources. Still, set a budget alert (step 9).

Do the steps in this order. **Don't delete anything on Render until step 10.**

---

## 1. Resource group

Azure portal → **Resource groups** → **Create**: name `teamforge`, region **Central India** (or
the region nearest to your users). Use the same region for everything below. If a later step
fails with a *policy* error, your student subscription doesn't allow that region: pick another
(for example East US) and use it for everything.

## 2. Database: Azure Database for PostgreSQL Flexible Server

**Create a resource** → *Azure Database for PostgreSQL Flexible Server* → **Create**.

* **Basics:**
  * Resource group `teamforge`.
  * Server name, for example `teamforge-db-<yourname>`; it must be unique across Azure.
  * Region as in step 1, PostgreSQL version **16**, workload type **Development**.
* **Compute + storage** → *Configure server*:
  * **Burstable**, **Standard_B1ms**.
  * Storage **32 GiB**, storage autogrow **off**.
  * High availability **off**. Keep the defaults for backup (7 days, geo-redundancy off).
  * **Save.** The cost estimate on the page should show the free offer. If it shows a monthly
    price, check the size again.
* **Authentication**: *PostgreSQL authentication only*.
  * Admin user `tfadmin`.
  * Choose a strong password **of letters and digits only**: symbols like `@ : / ? # %` would
    need escaping in the URL below.
* **Networking**:
  * **Public access**.
  * Tick **Allow public access from any Azure service within Azure to this server**. Container
    Apps and Cloud Shell reach it that way.
  * Add no other IP addresses.
* **Review + create.** It takes a few minutes.

Then open the server → **Databases** → **Add** → name `teamforge` → Save.

Your database URL (keep it private; it contains the password):

```
postgresql://tfadmin:<password>@<server name>.postgres.database.azure.com:5432/teamforge?sslmode=require
```

## 3. Copy the data from Render

Do this **before** the app first starts on Azure. On its first start the server fills an empty
database with the sample data, and the copy tool refuses to copy into a database that has data.
(If that happened, add `--replace` to the copy command; it empties the Azure database first.)

1. Tell your team not to use the Render site for the next 15 minutes. Changes made there after
   the copy would not move.
2. Render dashboard → your PostgreSQL → **Connect** → copy the **External Database URL**. If the
   free Render database has already expired, skip this step; Azure starts from the sample data.
3. Azure portal → the **Cloud Shell** icon (`>_`, top bar) → **Bash**. Then:

```bash
git clone --depth 1 https://github.com/anubhav8it/TeamForge.git && cd TeamForge/Phase-2
pip install --user "psycopg[binary]"
read -rs TEAMFORGE_COPY_FROM && export TEAMFORGE_COPY_FROM   # paste Render's URL, press Enter (nothing shows)
read -rs TEAMFORGE_COPY_TO && export TEAMFORGE_COPY_TO       # paste the Azure URL from step 2
python3 web/tools/copy_database.py
```

It prints the row counts it copied, for example
`Copied: users 12, sessions 30, shared_files 6, user_files 9`. It never prints passwords or
data. The URLs are read with `read -s`, so they are not saved in the shell history. Accounts,
password hashes and sessions are copied unchanged, so everyone signs in as before.

## 4. Document Intelligence (photos and scans of resumes; optional)

**Create a resource** → *Document Intelligence* → resource group `teamforge`, same region, a
name, pricing tier **Free F0** → **Review + create**. Open it → **Keys and Endpoint**: you need
the Endpoint and KEY 1 in step 6.

## 5. The container image

1. Merge the pull request that adds this file. After **Build web app** finishes, the **Deploy to
   Azure** workflow runs (Actions tab). You can also start it with **Run workflow**. It builds
   the image `ghcr.io/<your GitHub name, lower case>/teamforge-web` and tags it `latest`.
2. Let Azure pull it without a password. On GitHub, open your profile → **Packages** →
   `teamforge-web` → **Package settings** → **Change visibility** → **Public**. The image holds
   only `web/` and `data/` (the server, the built app and the sample data); no secrets go into
   it. If the repository is private and you want the image private too: create a GitHub token
   with only `read:packages`, then in step 6 choose image type *Private* with registry `ghcr.io`,
   your GitHub name and that token.

## 6. The Container App

**Create a resource** → *Container App* → **Create**.

* **Basics**:
  * Resource group `teamforge`, name `teamforge`, same region.
  * **Container Apps environment** → *Create new*, plan **Consumption**.
  * Monitoring: keep the suggested Log Analytics workspace. The server writes only a few MB of
    logs a month; Log Analytics bills by data ingested.
* **Container**:
  * Untick *Use quickstart image*.
  * Image source **Docker Hub or other registries**, image type **Public**.
  * Registry login server `ghcr.io`, image and tag `<your GitHub name>/teamforge-web:latest`.
  * CPU and memory **0.25 CPU cores, 0.5 Gi memory**.
  * Environment variables: `ADMIN_EMAILS` = your email (comma-separated for several). Add the
    secret ones after creation (below).
* **Ingress**:
  * **Enabled**, *Accepting traffic from anywhere*, ingress type **HTTP**.
  * **Target port 8000**. Leave *insecure connections* unticked, so `http://` is sent to `https://`.
* **Review + create.**

Then, in the new app:

1. **Settings → Secrets → Add**:
   * `database-url` = the URL from step 2.
   * Optionally `docintel-key` = KEY 1 from step 4.
2. **Application → Containers → Edit and deploy** → select the container → **Environment
   variables** → add:
   * `DATABASE_URL`, source *Reference a secret*, `database-url`.
   * Optionally `AZURE_DOCINTEL_KEY`, source *Reference a secret*, `docintel-key`.
   * Optionally `AZURE_DOCINTEL_ENDPOINT`, a manual entry with the endpoint.

   On the **Health probes** tab you can add liveness and readiness probes: HTTP, path
   `/healthz`, port 8000. Then **Save** → **Create**.
3. **Application → Scale**: min replicas **0**, max replicas **1** → Save. Zero means it sleeps
   when unused (free); the first visit after a quiet spell waits a few seconds while it starts.

Open **Overview → Application Url** (`https://teamforge.<…>.azurecontainerapps.io`). The sign-in
page should appear; sign in with an existing account.

**Log stream** (under Monitoring) shows the server log. It should say `(PostgreSQL)` on start,
and not `ADMIN_EMAILS is not set`.

## 7. Your own domain

1. **Get a domain.** The GitHub Student Developer Pack (education.github.com/pack) has included
   free first-year domains from some registrars; check its current offers. Otherwise buy one
   from any registrar. Renewal after the first year is not free.
2. Choose the address, for example `app.yourdomain.me` (a subdomain is the simplest) or
   `www.yourdomain.me`.
3. Container App → **Settings → Custom domains** → **Add custom domain** → **Managed
   certificate** → enter the address. The portal shows two DNS records to create, for example:

   | Type | Host / name | Value |
   |---|---|---|
   | CNAME | `app` | `teamforge.<…>.azurecontainerapps.io` |
   | TXT | `asuid.app` | the verification code shown |

4. Add both records in your registrar's **DNS** page. Wait a few minutes, then **Validate** →
   **Add**.

Azure issues and renews the HTTPS certificate for free. Your site is then
`https://app.yourdomain.me`, on any device.

For the bare domain (`yourdomain.me` without `app.`), the portal asks for an **A** record to
the environment's IP instead of the CNAME. Some registrars also offer forwarding from the bare
domain to `app.`.

## 8. Automatic deploys (optional)

Without this, each **Deploy to Azure** run builds the image and its summary shows one Cloud
Shell command that rolls it out (`az containerapp update … --image …`). To make it automatic:

1. Cloud Shell:

   ```bash
   az ad sp create-for-rbac --name teamforge-deploy --role contributor \
     --scopes $(az group show --name teamforge --query id --output tsv) --json-auth
   ```

   If it answers *insufficient privileges*, your university doesn't allow this; keep deploying
   with the command. The JSON it prints contains a secret: paste it only into the next step,
   then clear the screen (`clear`).
2. GitHub → repository → **Settings → Secrets and variables → Actions**:
   * Secret `AZURE_CREDENTIALS` = that JSON.
   * Variables `AZURE_CONTAINER_APP` = `teamforge` and `AZURE_RESOURCE_GROUP` = `teamforge`.

From then on, every merge to main builds the app, then the image, then rolls it out and checks
`/healthz`.

## 9. A budget alert

**Cost Management → Budgets → Add**: scope your subscription, amount $5 a month, an alert at
80% to your email. Nothing above should cost anything, so any alert means a setting to look at.

## 10. Check, then retire Render

On the Azure address (or your domain), check that:
- `/healthz` shows `ok`;
- existing accounts sign in and see their profiles and teams;
- a change you save is still there after reloading, and after signing in from another browser;
- a resume uploads (and a photo, if step 4 was done);
- the Admin page opens for an `ADMIN_EMAILS` account.

Keep Render running for a few days while people move to the new address. Then, on Render:
delete the web service, and delete the database last.

**After 12 months** the database's free period ends: a few weeks before, decide whether to keep
it (it will use credit), export it with `copy_database.py` to a new server, or delete it.
