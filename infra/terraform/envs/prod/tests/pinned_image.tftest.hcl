# The deployed server image comes from the pinned default, not from :latest,
# so republished images never replace the server by themselves.

mock_provider "oci" {
  mock_data "oci_identity_availability_domains" {
    defaults = {
      availability_domains = [{ name = "Uocm:US-ASHBURN-AD-1" }]
    }
  }
  mock_data "oci_core_images" {
    defaults = {
      images = [{ id = "ocid1.image.oc1..ubuntu2404aarch64" }]
    }
  }
  mock_resource "oci_core_instance" {
    defaults = {
      public_ip = "198.51.100.42"
    }
  }
}

variables {
  region            = "us-ashburn-1"
  compartment_ocid  = "ocid1.tenancy.oc1..test"
  admin_cidr        = "203.0.113.7/32"
  ssh_public_key    = "ssh-ed25519 AAAATEST test@example"
  duckdns_subdomain = "vw-friends"
  duckdns_token     = "01234567-89ab-cdef-0123-456789abcdef"
}

run "deploys_the_pinned_image_by_default" {
  command = plan

  assert {
    condition     = can(regex("^ghcr\\.io/irregularthanis/ggpo-matchmaking@sha256:[0-9a-f]{64}$", var.container_image))
    error_message = "The default container_image must be a digest-pinned server image."
  }
}
