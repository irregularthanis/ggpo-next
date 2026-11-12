# Wiring tests for the production root module. OCI is mocked; nothing is
# created. Real apply and destroy are run by hand (see infra/terraform/README.md).

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
  container_image   = "ghcr.io/irregularthanis/ggpo-matchmaking@sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
  admin_cidr        = "203.0.113.7/32"
  ssh_public_key    = "ssh-ed25519 AAAATEST test@example"
  duckdns_subdomain = "vw-friends"
  duckdns_token     = "01234567-89ab-cdef-0123-456789abcdef"
}

run "applied_state_exposes_the_address_vector_war_uses" {
  command = apply

  assert {
    condition     = output.server_address == "vw-friends.duckdns.org:7100"
    error_message = "server_address must be the DuckDNS name and the UDP port."
  }
  assert {
    condition     = output.public_ip == "198.51.100.42"
    error_message = "public_ip must come from the instance in state."
  }
  assert {
    condition     = output.server_port == 7100
    error_message = "server_port must default to 7100."
  }
}

run "custom_port_flows_to_the_address" {
  command = apply
  variables {
    server_port = 7200
  }

  assert {
    condition     = output.server_address == "vw-friends.duckdns.org:7200"
    error_message = "A custom port must reach server_address."
  }
}
