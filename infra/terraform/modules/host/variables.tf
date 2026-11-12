variable "compartment_ocid" {
  description = "Compartment for every resource."
  type        = string
}

variable "name" {
  description = "Prefix for resource display names."
  type        = string
  default     = "ggpo-matchmaking"
}

variable "container_image" {
  description = "Server image pinned by digest, e.g. ghcr.io/owner/ggpo-matchmaking@sha256:<64 hex>."
  type        = string
  validation {
    condition     = can(regex("^[a-z0-9./_-]+(:[A-Za-z0-9._-]+)?@sha256:[0-9a-f]{64}$", var.container_image))
    error_message = "container_image must be pinned by digest (…@sha256:<64 hex digits>)."
  }
}

variable "server_port" {
  description = "Public UDP port of the matchmaking server."
  type        = number
  default     = 7100
  validation {
    condition     = var.server_port >= 1024 && var.server_port <= 65535 && floor(var.server_port) == var.server_port
    error_message = "server_port must be an integer from 1024 to 65535."
  }
}

variable "relay_port_range" {
  description = "Public UDP ports the relay forwards game traffic on, one per player per session."
  type = object({
    first = number
    last  = number
  })
  default = { first = 7101, last = 7499 }
  validation {
    condition = (
      var.relay_port_range.first >= 1024 && var.relay_port_range.last <= 65535 &&
      var.relay_port_range.first <= var.relay_port_range.last &&
      var.relay_port_range.last - var.relay_port_range.first < 1000
    )
    error_message = "relay_port_range must be at most 1000 ports within 1024-65535, with first <= last."
  }
}

variable "admin_cidr" {
  description = "Only source allowed to reach SSH, e.g. your home IP as x.x.x.x/32."
  type        = string
  validation {
    condition     = can(cidrhost(var.admin_cidr, 0)) && tonumber(split("/", var.admin_cidr)[1]) >= 16
    error_message = "admin_cidr must be a valid IPv4 CIDR no wider than /16 (never 0.0.0.0/0)."
  }
}

variable "ssh_public_key" {
  description = "Public key for the ubuntu user."
  type        = string
}

variable "ocpus" {
  description = "Ampere A1 OCPUs; the always-free allowance is 2 in total."
  type        = number
  default     = 1
  validation {
    condition     = var.ocpus >= 1 && var.ocpus <= 2
    error_message = "ocpus must be 1 or 2 to stay inside the always-free allowance."
  }
}

variable "memory_gbs" {
  description = "Ampere A1 memory in GB; the always-free allowance is 12 in total."
  type        = number
  default     = 6
  validation {
    condition     = var.memory_gbs >= 1 && var.memory_gbs <= 12
    error_message = "memory_gbs must be from 1 to 12 to stay inside the always-free allowance."
  }
}

variable "extra_cloud_init" {
  description = "Additional cloud-config parts (for example the DuckDNS updater), merged into user data."
  type = list(object({
    filename = string
    content  = string
  }))
  default = []
}
