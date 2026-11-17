# A monthly budget over the whole tenancy with two alerts, so a Pay As You Go
# account that leaves the always-free allowance is noticed at once. Budgets
# only alert: they never stop resources or block charges.

resource "oci_budget_budget" "tenancy" {
  compartment_id         = var.tenancy_ocid
  display_name           = "${var.name}-free-tier-guard"
  description            = "Alerts when the tenancy spends anything beyond the always-free allowance."
  amount                 = var.monthly_amount
  reset_period           = "MONTHLY"
  processing_period_type = "MONTH"
  target_type            = "COMPARTMENT"
  targets                = [var.tenancy_ocid]
}

# Any real spend: the first cent means something is outside always-free.
resource "oci_budget_alert_rule" "actual_spend" {
  budget_id      = oci_budget_budget.tenancy.id
  display_name   = "${var.name}-any-actual-spend"
  type           = "ACTUAL"
  threshold_type = "ABSOLUTE"
  threshold      = 0.01
  recipients     = var.alert_email
  message        = "Oracle Cloud is charging money. Check the matchmaking server and run terraform destroy if it is no longer needed."
}

# Early warning before the month's spend is actually incurred.
resource "oci_budget_alert_rule" "forecast" {
  budget_id      = oci_budget_budget.tenancy.id
  display_name   = "${var.name}-forecast-over-budget"
  type           = "FORECAST"
  threshold_type = "PERCENTAGE"
  threshold      = 100
  recipients     = var.alert_email
  message        = "Oracle Cloud forecasts spend above the monthly budget."
}
