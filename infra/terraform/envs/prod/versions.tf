terraform {
  required_version = ">= 1.7"

  required_providers {
    oci = {
      source  = "oracle/oci"
      version = "~> 9.0"
    }
    cloudinit = {
      source  = "hashicorp/cloudinit"
      version = "~> 2.3"
    }
  }

  # State lives in Oracle's free Object Storage through its S3-compatible API,
  # so a later destroy works from any machine. Settings come from a git-ignored
  # backend.hcl (see backend.hcl.example): terraform init -backend-config=backend.hcl
  backend "s3" {}
}

provider "oci" {
  # Credentials come from ~/.oci/config (or OCI_* environment variables),
  # never from this repository.
  config_file_profile = var.oci_profile
  region              = var.region
}
