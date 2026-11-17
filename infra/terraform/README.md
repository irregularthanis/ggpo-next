# Matchmaking server infrastructure

Terraform for the Vector War lobby and relay server: one always-free Oracle
Cloud Ampere A1 VM that runs the server container, and a free DuckDNS name
pointing at it. Trusted pull requests validate a real plan; infrastructure or
server changes merged to `master` apply automatically and verify DuckDNS plus
the lobby endpoint. The manual **Deploy matchmaking server** workflow remains
available for explicit plan, apply and destroy operations.

Plan and decisions: [internet play technical plan](../../doc/planning/2026-11-11-internet_play/TECH_PLAN.md).

| Path | Contents |
| --- | --- |
| [`modules/host`](modules/host) | Network, firewall (public UDP lobby port and relay range, key-only SSH from `admin_cidr`), VM, and cloud-init that runs the container |
| [`modules/duckdns`](modules/duckdns) | No cloud resources: renders the cloud-init part that keeps the DuckDNS name pointing at the VM |
| [`envs/prod`](envs/prod) | Provider, remote state, variables and wiring; the directory you apply from |

## One-time setup

1. **Oracle Cloud:** create an Always Free account and note your home region;
   always-free compute must be created there.
2. **API key:** in the console, add an API key to your user and save the
   generated config as `~/.oci/config` (profile `DEFAULT`, or set
   `oci_profile`).
3. **State bucket:** create an Object Storage bucket (for example
   `ggpo-terraform-state`) with versioning on, and a Customer Secret Key for its
   S3-compatible API. Copy `envs/prod/backend.hcl.example` to `backend.hcl`,
   fill in the bucket, region and namespace, and export the key as
   `AWS_ACCESS_KEY_ID` and `AWS_SECRET_ACCESS_KEY`.
4. **DuckDNS:** sign in at duckdns.org, create a subdomain, and export your
   token as `TF_VAR_duckdns_token`.
5. **Variables:** copy `envs/prod/terraform.tfvars.example` to
   `terraform.tfvars` and fill it in. `container_image` is the digest printed in
   the summary of the latest `Infrastructure` workflow run on `master`.

`backend.hcl`, `terraform.tfvars` and all state files are git-ignored. Never
commit them.

## Bring the server up and down

```bash
cd infra/terraform/envs/prod
terraform init -backend-config=backend.hcl
terraform apply           # review the plan, then confirm
terraform output server_address   # e.g. vw-friends.duckdns.org:7100

# ... play ...

terraform destroy
```

The DuckDNS name stays reserved after `destroy`. While the server is down,
Vector War's Matchmaking reports that the server is unreachable, and direct
`IP:PORT` play still works. Allow a minute or two after `apply` for the VM to
install Docker, start the container and update DuckDNS.

## Deploy from GitHub Actions

The **Deploy matchmaking server** workflow
([`.github/workflows/deploy-matchmaking.yml`](../../.github/workflows/deploy-matchmaking.yml))
does the same from the Actions tab: **Run workflow** and pick an action.

| Action | Effect |
| --- | --- |
| `apply` | Creates or updates the server, waits until the DuckDNS name resolves to the VM (the VM updates its own record at boot and every 5 minutes) and the server answers on the lobby port, then prints the address in the run summary |
| `plan` | Shows what `apply` would change, without changing anything |
| `destroy` | Tears everything down and clears the DuckDNS record, so the name stops resolving to an IP that may be reassigned |

The same deployment workflow is reusable: the Infrastructure workflow calls
`plan` for repository-owner pull requests and `apply` after matching changes
merge to `master`. Direct manual runs expose `plan`, `apply` and `destroy`.
Runs are serialized, so operations never race on the state. By default it
deploys the current `:latest` image pinned by its digest; the
`container_image` input overrides that. The GHCR package must be public,
because the VM pulls it without credentials.

Pull-request plans use a non-deployable placeholder digest. The image lane
separately builds and smoke-tests the candidate, while the plan qualifies the
cloud resources without requiring an image published only after merge. On
`master`, CI publishes the tested image and resolves its real digest before
applying Terraform.

After a `master` apply and its DNS/lobby health check, the Infrastructure
workflow calls [`public-multiplatform-game.yml`](../../.github/workflows/public-multiplatform-game.yml)
in required mode. Linux, Windows/MSVC and Android join one run-scoped Vector War lobby
through the DuckDNS name and keep every peer path on the deployed relay. The
standalone workflow defaults to optional mode: if no server answers the
preflight probe, it reports a neutral skip before building or launching peers.

Infrastructure CI is path-filtered to Terraform, matchmaking-server and
workflow changes; documentation-only changes do not start it. Pull requests
from forks or non-owner branches never receive credentials and run only the
mocked validation lanes. Manual dispatch of **Infrastructure** reruns those
validation lanes without applying anything.

Configure the following repository Actions settings under **Settings → Secrets
and variables → Actions**. Secrets belong under **Secrets** and variables under
**Variables**:

| Kind | Name | Value |
| --- | --- | --- |
| Secret | `OCI_REGION` | Home region, e.g. `us-ashburn-1` |
| Secret | `OCI_TENANCY_OCID` | Tenancy OCID |
| Secret | `OCI_USER_OCID` | User that owns the API key |
| Secret | `OCI_FINGERPRINT` | API key fingerprint |
| Secret | `OCI_PRIVATE_KEY` | API private key (PEM, whole file) |
| Secret | `OCI_COMPARTMENT_OCID` | Compartment (the tenancy OCID is fine) |
| Variable | `STATE_BUCKET` | Object Storage bucket for the state |
| Variable | `STATE_NAMESPACE` | Object Storage namespace |
| Secret | `STATE_ACCESS_KEY_ID` | Customer Secret Key ID (S3-compatible API) |
| Secret | `STATE_SECRET_ACCESS_KEY` | Customer Secret Key secret |
| Secret | `ADMIN_CIDR` | IPv4 CIDR allowed to reach key-only SSH; prefer your public IP as `x.x.x.x/32`, while `0.0.0.0/0` explicitly permits every source |
| Secret | `SSH_PUBLIC_KEY` | Public key for SSH |
| Variable | `DUCKDNS_SUBDOMAIN` | Subdomain without `.duckdns.org` |
| Secret | `DUCKDNS_TOKEN` | DuckDNS account token |

The workflow checks that all values are set before touching Terraform, writes
the OCI key and backend settings only for the run, and deletes them at the end.
State lives in the same bucket as local runs, so you can apply from Actions and
destroy locally, or the other way round.

The job still targets the `matchmaking` GitHub environment as a deployment
boundary. It does not require environment-scoped values; optionally configure
required reviewers there to approve each deployment.

## Tests

No account or credentials are needed:

```bash
cd infra/terraform
terraform fmt -check -recursive
for dir in modules/duckdns modules/host envs/prod; do
  terraform -chdir=$dir init -backend=false
  terraform -chdir=$dir test
done
tflint --init --config "$PWD/.tflint.hcl"
tflint --recursive --config "$PWD/.tflint.hcl"
```

| Test file | What it proves |
| --- | --- |
| [`modules/host/tests/host.tftest.hcl`](modules/host/tests/host.tftest.hcl) | The UDP lobby and relay range are public and key-only SSH follows `admin_cidr`, including an explicitly requested `0.0.0.0/0`; malformed CIDRs, a relay range containing the lobby port, and ranges larger than 1000 ports are rejected; the VM is the always-free A1 shape; cloud-init runs the image by digest with the relay range, read-only on the host network, opens both in iptables, and merges extra parts; after a mocked apply, the instance, subnet, route and outputs are wired in state |
| [`modules/duckdns/tests/duckdns.tftest.hcl`](modules/duckdns/tests/duckdns.tftest.hcl) | The update URL names the subdomain and lets DuckDNS use the server's own IP; refresh at boot and every 5 minutes; the token file is root-only; bad names and tokens are rejected |
| [`envs/prod/tests/prod.tftest.hcl`](envs/prod/tests/prod.tftest.hcl) | After a mocked apply, state exposes the `name:port` Vector War uses, the relay range and the instance IP, including with a custom port |

The `Infrastructure` workflow runs all of these, the matchmaking server tests,
and a multi-architecture image build on every code change; its aggregate check
is **Complete infrastructure checks**.

## Cost guardrails

- [`modules/budget`](modules/budget): a monthly budget over the whole tenancy
  (default 1 in the account currency) that emails `budget_alert_email` on the
  first cent of actual spend and when the month's forecast exceeds the budget.
  It is created and destroyed with the server; leave `budget_alert_email` empty
  to skip it. In CI, set the `BUDGET_ALERT_EMAIL` secret. Budgets only alert:
  they never stop resources.

- `shape` accepts only the always-free `VM.Standard.A1.Flex` (default) and
  `VM.Standard.E2.1.Micro`; when Oracle reports "Out of host capacity", the
  apply workflow retries every availability domain with A1, then with the AMD
  micro shape (same multi-arch image). Locally, pass
  `-var shape=VM.Standard.E2.1.Micro` or `-var availability_domain_index=1`.
- Variable validation keeps the VM inside the always-free Ampere A1 allowance
  (at most 2 OCPUs and 12 GB).
- One VCN, one public IP, no load balancer, no block volume beyond the boot
  volume.
- Relay traffic counts against the free 10 TB per month of outbound data;
  Vector War sends a few small packets per frame per player.
- Run `terraform destroy` when you are done; nothing is left running.
