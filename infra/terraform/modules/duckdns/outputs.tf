output "fqdn" {
  description = "Public name of the matchmaking server."
  value       = local.fqdn
}

output "cloud_init_part" {
  description = "cloud-config part for the host module's extra_cloud_init."
  sensitive   = true
  value = {
    filename = "duckdns.yaml"
    content  = local.cloud_init
  }
}
