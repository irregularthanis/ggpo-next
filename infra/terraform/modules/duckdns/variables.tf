variable "subdomain" {
  description = "DuckDNS subdomain, without .duckdns.org."
  type        = string
  validation {
    condition     = can(regex("^[a-z0-9][a-z0-9-]{0,61}[a-z0-9]$", var.subdomain))
    error_message = "subdomain must be 2-63 lowercase letters, digits or hyphens, not starting or ending with a hyphen."
  }
}

variable "token" {
  description = "DuckDNS account token."
  type        = string
  sensitive   = true
  validation {
    condition     = can(regex("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$", var.token))
    error_message = "token must be a DuckDNS token (a UUID)."
  }
}

variable "interval_minutes" {
  description = "How often the server refreshes its DuckDNS record."
  type        = number
  default     = 5
  validation {
    condition     = var.interval_minutes >= 1 && var.interval_minutes <= 60
    error_message = "interval_minutes must be from 1 to 60."
  }
}
