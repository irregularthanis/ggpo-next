terraform {
  required_version = ">= 1.7"
  required_providers {
    oci = {
      source  = "oracle/oci"
      version = ">= 9.0, < 10.0"
    }
    cloudinit = {
      source  = "hashicorp/cloudinit"
      version = ">= 2.3, < 3.0"
    }
  }
}
