# One always-free Ampere A1 VM whose only job is running the matchmaking
# container, in its own public subnet.

locals {
  shape     = "VM.Standard.A1.Flex"
  vcn_cidr  = "10.71.0.0/16"
  subnet    = "10.71.1.0/24"
  container = "${var.name}-server"
}

data "oci_identity_availability_domains" "all" {
  compartment_id = var.compartment_ocid
}

data "oci_core_images" "ubuntu" {
  compartment_id           = var.compartment_ocid
  operating_system         = "Canonical Ubuntu"
  operating_system_version = "24.04"
  shape                    = local.shape
  sort_by                  = "TIMECREATED"
  sort_order               = "DESC"
}

resource "oci_core_vcn" "main" {
  compartment_id = var.compartment_ocid
  cidr_blocks    = [local.vcn_cidr]
  display_name   = "${var.name}-vcn"
}

resource "oci_core_internet_gateway" "main" {
  compartment_id = var.compartment_ocid
  vcn_id         = oci_core_vcn.main.id
  display_name   = "${var.name}-igw"
  enabled        = true
}

resource "oci_core_route_table" "public" {
  compartment_id = var.compartment_ocid
  vcn_id         = oci_core_vcn.main.id
  display_name   = "${var.name}-public"
  route_rules {
    destination       = "0.0.0.0/0"
    destination_type  = "CIDR_BLOCK"
    network_entity_id = oci_core_internet_gateway.main.id
  }
}

resource "oci_core_security_list" "server" {
  compartment_id = var.compartment_ocid
  vcn_id         = oci_core_vcn.main.id
  display_name   = "${var.name}-server"

  egress_security_rules {
    destination = "0.0.0.0/0"
    protocol    = "all"
  }

  # Matchmaking: UDP from anywhere, only on the server port.
  ingress_security_rules {
    protocol    = "17"
    source      = "0.0.0.0/0"
    description = "Vector War matchmaking"
    udp_options {
      min = var.server_port
      max = var.server_port
    }
  }

  # Administration: SSH only from the maintainer.
  ingress_security_rules {
    protocol    = "6"
    source      = var.admin_cidr
    description = "SSH from the maintainer"
    tcp_options {
      min = 22
      max = 22
    }
  }
}

resource "oci_core_subnet" "public" {
  compartment_id             = var.compartment_ocid
  vcn_id                     = oci_core_vcn.main.id
  cidr_block                 = local.subnet
  display_name               = "${var.name}-public"
  route_table_id             = oci_core_route_table.public.id
  security_list_ids          = [oci_core_security_list.server.id]
  prohibit_public_ip_on_vnic = false
}

data "cloudinit_config" "server" {
  gzip          = false
  base64_encode = true

  part {
    filename     = "server.yaml"
    content_type = "text/cloud-config"
    content = templatefile("${path.module}/server.cloud-init.yaml.tftpl", {
      image     = var.container_image
      port      = var.server_port
      container = local.container
    })
  }

  dynamic "part" {
    for_each = var.extra_cloud_init
    content {
      filename     = part.value.filename
      content_type = "text/cloud-config"
      merge_type   = "list(append)+dict(no_replace,recurse_list)+str()"
      content      = part.value.content
    }
  }
}

resource "oci_core_instance" "server" {
  compartment_id      = var.compartment_ocid
  availability_domain = data.oci_identity_availability_domains.all.availability_domains[0].name
  display_name        = var.name
  shape               = local.shape

  shape_config {
    ocpus         = var.ocpus
    memory_in_gbs = var.memory_gbs
  }

  source_details {
    source_type = "image"
    source_id   = data.oci_core_images.ubuntu.images[0].id
  }

  create_vnic_details {
    subnet_id        = oci_core_subnet.public.id
    assign_public_ip = true
  }

  metadata = {
    ssh_authorized_keys = var.ssh_public_key
    user_data           = data.cloudinit_config.server.rendered
  }
}
