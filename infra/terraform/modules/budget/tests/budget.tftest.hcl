# Unit tests for the budget module. OCI is mocked; nothing is created.

mock_provider "oci" {}

variables {
  tenancy_ocid = "ocid1.tenancy.oc1..test"
  alert_email  = "owner@example.com"
}

run "budget_covers_the_whole_tenancy_monthly" {
  command = plan

  assert {
    condition     = oci_budget_budget.tenancy.compartment_id == var.tenancy_ocid && oci_budget_budget.tenancy.targets == tolist([var.tenancy_ocid])
    error_message = "The budget must live in, and target, the root compartment."
  }
  assert {
    condition     = oci_budget_budget.tenancy.target_type == "COMPARTMENT" && oci_budget_budget.tenancy.reset_period == "MONTHLY"
    error_message = "The budget must be a monthly compartment budget."
  }
  assert {
    condition     = oci_budget_budget.tenancy.amount == 1
    error_message = "The default budget must be 1."
  }
}

run "alerts_fire_on_the_first_cent_and_on_forecast" {
  command = plan

  assert {
    condition     = oci_budget_alert_rule.actual_spend.type == "ACTUAL" && oci_budget_alert_rule.actual_spend.threshold_type == "ABSOLUTE" && oci_budget_alert_rule.actual_spend.threshold == 0.01
    error_message = "Any actual spend must alert."
  }
  assert {
    condition     = oci_budget_alert_rule.forecast.type == "FORECAST" && oci_budget_alert_rule.forecast.threshold == 100
    error_message = "A forecast over the budget must alert."
  }
  assert {
    condition     = oci_budget_alert_rule.actual_spend.recipients == "owner@example.com" && oci_budget_alert_rule.forecast.recipients == "owner@example.com"
    error_message = "Both alerts must go to alert_email."
  }
}

run "alerts_belong_to_the_budget_in_state" {
  command = apply

  assert {
    condition     = oci_budget_alert_rule.actual_spend.budget_id == oci_budget_budget.tenancy.id && oci_budget_alert_rule.forecast.budget_id == oci_budget_budget.tenancy.id
    error_message = "Both alert rules must attach to the budget."
  }
  assert {
    condition     = output.budget_id == oci_budget_budget.tenancy.id
    error_message = "budget_id must come from state."
  }
}

run "rejects_a_compartment_instead_of_the_tenancy" {
  command = plan
  variables {
    tenancy_ocid = "ocid1.compartment.oc1..test"
  }
  expect_failures = [var.tenancy_ocid]
}

run "rejects_an_invalid_email" {
  command = plan
  variables {
    alert_email = "not-an-email"
  }
  expect_failures = [var.alert_email]
}

run "rejects_a_zero_budget" {
  command = plan
  variables {
    monthly_amount = 0
  }
  expect_failures = [var.monthly_amount]
}
