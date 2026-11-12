# Unit tests for the host module. The OCI provider is mocked, so no account or
# credentials are needed; cloudinit is a local provider and renders for real.

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
      public_ip = "192.0.2.10"
    }
  }
}

variables {
  compartment_ocid = "ocid1.tenancy.oc1..test"
  container_image  = "ghcr.io/irregularthanis/ggpo-matchmaking@sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
  admin_cidr       = "203.0.113.7/32"
  ssh_public_key   = "ssh-ed25519 AAAATEST test@example"
  extra_cloud_init = [{ filename = "extra.yaml", content = "#cloud-config\nruncmd:\n  - echo extra-part-marker\n" }]
}

run "firewall_opens_only_the_lobby_and_relay_ports_publicly_and_ssh_to_admin" {
  # The rule set has provider-computed fields, so it is only known after a
  # (mocked) apply.
  command = apply

  assert {
    condition     = length(oci_core_security_list.server.ingress_security_rules) == 3
    error_message = "Exactly three ingress rules are expected: lobby UDP, relay UDP range and admin SSH."
  }
  assert {
    condition = alltrue([
      for rule in oci_core_security_list.server.ingress_security_rules :
      rule.source != "0.0.0.0/0" || (rule.protocol == "17" && (
        (try(one(rule.udp_options).min, 0) == 7100 && try(one(rule.udp_options).max, 0) == 7100) ||
        (try(one(rule.udp_options).min, 0) == 7101 && try(one(rule.udp_options).max, 0) == 7499)
      ))
    ])
    error_message = "Only UDP 7100 and the relay range 7101-7499 may be open to 0.0.0.0/0."
  }
  assert {
    condition = length([
      for rule in oci_core_security_list.server.ingress_security_rules : rule
      if rule.protocol == "6" && rule.source == "203.0.113.7/32" && try(one(rule.tcp_options).min, 0) == 22 && try(one(rule.tcp_options).max, 0) == 22
    ]) == 1
    error_message = "SSH must be open only to admin_cidr."
  }
}

run "vm_stays_inside_the_always_free_allowance" {
  command = plan

  assert {
    condition     = oci_core_instance.server.shape == "VM.Standard.A1.Flex"
    error_message = "The server must use the always-free Ampere A1 shape."
  }
  assert {
    condition     = one(oci_core_instance.server.shape_config).ocpus == 1 && one(oci_core_instance.server.shape_config).memory_in_gbs == 6
    error_message = "Default shape must be 1 OCPU and 6 GB."
  }
  assert {
    condition     = one(oci_core_instance.server.create_vnic_details).assign_public_ip == "true"
    error_message = "The server needs a public IP."
  }
}

run "cloud_init_runs_the_pinned_image_on_the_host_network" {
  command = plan

  assert {
    condition     = strcontains(base64decode(oci_core_instance.server.metadata["user_data"]), var.container_image)
    error_message = "cloud-init must run the image pinned by digest."
  }
  assert {
    condition     = strcontains(base64decode(oci_core_instance.server.metadata["user_data"]), "--network host --read-only --cap-drop ALL")
    error_message = "The container must run read-only, without capabilities, on the host network."
  }
  assert {
    condition     = strcontains(base64decode(oci_core_instance.server.metadata["user_data"]), "iptables -I INPUT 1 -p udp --dport 7100 -j ACCEPT")
    error_message = "cloud-init must open the server port in the image's iptables rules."
  }
  assert {
    condition     = strcontains(base64decode(oci_core_instance.server.metadata["user_data"]), "iptables -I INPUT 1 -p udp --dport 7101:7499 -j ACCEPT")
    error_message = "cloud-init must open the relay range in the image's iptables rules."
  }
  assert {
    condition     = strcontains(base64decode(oci_core_instance.server.metadata["user_data"]), "--relay-ports 7101-7499")
    error_message = "The server must be started with the relay range."
  }
  assert {
    condition     = strcontains(base64decode(oci_core_instance.server.metadata["user_data"]), "echo extra-part-marker")
    error_message = "Extra cloud-init parts must be merged into user data."
  }
}

run "applied_state_wires_network_and_outputs" {
  command = apply

  assert {
    condition     = output.public_ip == "192.0.2.10"
    error_message = "public_ip output must come from the instance in state."
  }
  assert {
    condition     = one(oci_core_instance.server.create_vnic_details).subnet_id == oci_core_subnet.public.id
    error_message = "The instance must be in the public subnet."
  }
  assert {
    condition     = oci_core_subnet.public.security_list_ids == toset([oci_core_security_list.server.id]) || tolist(oci_core_subnet.public.security_list_ids) == [oci_core_security_list.server.id]
    error_message = "The subnet must use only the server security list."
  }
  assert {
    condition     = one(oci_core_route_table.public.route_rules).network_entity_id == oci_core_internet_gateway.main.id
    error_message = "The default route must go to the internet gateway."
  }
  assert {
    condition     = oci_core_instance.server.availability_domain == "Uocm:US-ASHBURN-AD-1"
    error_message = "The instance must use the looked-up availability domain."
  }
}

run "rejects_tag_without_digest" {
  command = plan
  variables {
    container_image = "ghcr.io/irregularthanis/ggpo-matchmaking:latest"
  }
  expect_failures = [var.container_image]
}

run "rejects_world_open_ssh" {
  command = plan
  variables {
    admin_cidr = "0.0.0.0/0"
  }
  expect_failures = [var.admin_cidr]
}

run "rejects_privileged_or_invalid_port" {
  command = plan
  variables {
    server_port = 80
  }
  expect_failures = [var.server_port]
}

run "rejects_a_relay_range_that_contains_the_lobby_port" {
  command = plan
  variables {
    relay_port_range = { first = 7000, last = 7200 }
  }
  expect_failures = [oci_core_security_list.server]
}

run "rejects_an_oversized_or_inverted_relay_range" {
  command = plan
  variables {
    relay_port_range = { first = 8000, last = 9500 }
  }
  expect_failures = [var.relay_port_range]
}

run "rejects_shapes_beyond_the_free_allowance" {
  command = plan
  variables {
    ocpus      = 4
    memory_gbs = 24
  }
  expect_failures = [var.ocpus, var.memory_gbs]
}
