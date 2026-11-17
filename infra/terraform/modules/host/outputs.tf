output "public_ip" {
  description = "Public IPv4 address of the server."
  value       = oci_core_instance.server.public_ip
}

output "server_port" {
  description = "Public UDP port of the matchmaking server."
  value       = var.server_port
}

output "relay_port_range" {
  description = "Public UDP relay port range."
  value       = "${var.relay_port_range.first}-${var.relay_port_range.last}"
}

output "instance_id" {
  description = "OCID of the server instance."
  value       = oci_core_instance.server.id
}
