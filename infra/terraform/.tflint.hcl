# Terraform language rules; no cloud-specific ruleset is needed for OCI.
plugin "terraform" {
  enabled = true
  preset  = "recommended"
}
