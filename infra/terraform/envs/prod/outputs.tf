output "server_address" {
  description = "What Vector War's Matchmaking field should contain."
  value       = "${module.duckdns.fqdn}:${module.host.server_port}"
}

output "fqdn" {
  description = "DuckDNS name of the server."
  value       = module.duckdns.fqdn
}

output "public_ip" {
  description = "Public IPv4 address the DuckDNS name should resolve to."
  value       = module.host.public_ip
}

output "server_port" {
  description = "Public UDP port."
  value       = module.host.server_port
}
