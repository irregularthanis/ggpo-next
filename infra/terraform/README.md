# Matchmaking server infrastructure

Terraform for the Vector War lobby and relay server: one always-free Oracle
Cloud Ampere A1 VM that runs the server container, and a free DuckDNS name
pointing at it. You bring it up before friends play and destroy it afterwards,
locally or with the manual **Deploy matchmaking server** workflow. Automatic CI
never creates real infrastructure; it only runs the tests below against mocked
providers.

Plan and decisions: [internet play technical plan](../../doc/planning/2026-11-11-internet_play/TECH_PLAN.md).

| Path | Contents |
| --- | --- |
| [`modules/host`](modules/host) | Network, firewall (public UDP lobby port and relay range, SSH from `admin_cidr` only), VM, and cloud-init that runs the container |
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

It is manual only: never a required check and never run by pushes or pull
requests, so it blocks nothing. Runs are serialized, so `apply` and `destroy`
never race on the state. By default it deploys the current `:latest` image
pinned by its digest; the `container_image` input overrides that. The GHCR
package must be public, because the VM pulls it without credentials.

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
| Secret | `ADMIN_CIDR` | Your public IP as `x.x.x.x/32` (SSH only) |
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
| [`modules/host/tests/host.tftest.hcl`](modules/host/tests/host.tftest.hcl) | Only the UDP lobby port and relay range are open to the internet and SSH only to `admin_cidr`; a relay range containing the lobby port, or larger than 1000 ports, is rejected; the VM is the always-free A1 shape; cloud-init runs the image by digest with the relay range, read-only on the host network, opens both in iptables, and merges extra parts; after a mocked apply, the instance, subnet, route and outputs are wired in state; bad inputs are rejected |
| [`modules/duckdns/tests/duckdns.tftest.hcl`](modules/duckdns/tests/duckdns.tftest.hcl) | The update URL names the subdomain and lets DuckDNS use the server's own IP; refresh at boot and every 5 minutes; the token file is root-only; bad names and tokens are rejected |
| [`envs/prod/tests/prod.tftest.hcl`](envs/prod/tests/prod.tftest.hcl) | After a mocked apply, state exposes the `name:port` Vector War uses, the relay range and the instance IP, including with a custom port |

The `Infrastructure` workflow runs all of these, the matchmaking server tests,
and a multi-architecture image build on every code change; its aggregate check
is **Complete infrastructure checks**.

## Cost guardrails

- Variable validation keeps the VM inside the always-free Ampere A1 allowance
  (at most 2 OCPUs and 12 GB).
- One VCN, one public IP, no load balancer, no block volume beyond the boot
  volume.
- Relay traffic counts against the free 10 TB per month of outbound data;
  Vector War sends a few small packets per frame per player.
- Run `terraform destroy` when you are done; nothing is left running.
