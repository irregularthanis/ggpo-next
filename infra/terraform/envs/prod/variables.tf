variable "oci_profile" {
  description = "Profile in ~/.oci/config to authenticate with."
  type        = string
  default     = "DEFAULT"
}

variable "region" {
  description = "OCI home region; always-free compute must be created there."
  type        = string
}

variable "compartment_ocid" {
  description = "Compartment for every resource (the tenancy OCID is fine)."
  type        = string
}

variable "container_image" {
  description = "Matchmaking server image pinned by digest."
  type        = string
  # The deployed server image. Changing it replaces the VM (new public IP, a
  # short outage), so it only changes when you update this pin: copy the
  # digest from the "Record the digest to deploy" summary of an Infrastructure
  # run on master. Images published since then are not deployed until then.
  default = "ghcr.io/irregularthanis/ggpo-matchmaking@sha256:70efe1b26128247fb96c219cd426c909b4aec1d4eb2875733dfacda1f0a7be65"
}

variable "server_port" {
  description = "Public UDP port of the matchmaking server."
  type        = number
  default     = 7100
}

variable "relay_port_range" {
  description = "Public UDP relay port range."
  type = object({
    first = number
    last  = number
  })
  default = { first = 7101, last = 7499 }
}

variable "shape" {
  description = "Always-free shape; see modules/host."
  type        = string
  default     = "VM.Standard.A1.Flex"
}

variable "availability_domain_index" {
  description = "Availability domain to launch in; see modules/host."
  type        = number
  default     = 0
}

variable "admin_cidr" {
  description = "IPv4 CIDR allowed to reach key-only SSH; prefer your public IP as x.x.x.x/32."
  type        = string
}

variable "ssh_public_key" {
  description = "Public key for SSH access to the server."
  type        = string
}

variable "duckdns_subdomain" {
  description = "DuckDNS subdomain, without .duckdns.org."
  type        = string
}

variable "duckdns_token" {
  description = "DuckDNS account token."
  type        = string
  sensitive   = true
}
