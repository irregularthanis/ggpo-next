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

variable "admin_cidr" {
  description = "Your public IP as x.x.x.x/32; the only SSH source allowed."
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
