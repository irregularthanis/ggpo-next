variable "tenancy_ocid" {
  description = "Tenancy OCID. Budgets live in the root compartment and cover the whole tenancy."
  type        = string
  validation {
    condition     = startswith(var.tenancy_ocid, "ocid1.tenancy.")
    error_message = "tenancy_ocid must be a tenancy OCID (ocid1.tenancy...)."
  }
}

variable "alert_email" {
  description = "Address that receives the cost alerts."
  type        = string
  validation {
    condition     = can(regex("^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$", var.alert_email))
    error_message = "alert_email must be an email address."
  }
}

variable "monthly_amount" {
  description = "Monthly budget in the account currency; anything above zero means always-free was exceeded."
  type        = number
  default     = 1
  validation {
    condition     = var.monthly_amount > 0
    error_message = "monthly_amount must be greater than zero."
  }
}

variable "name" {
  description = "Prefix for display names."
  type        = string
  default     = "ggpo-matchmaking"
}
