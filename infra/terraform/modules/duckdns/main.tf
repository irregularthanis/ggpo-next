# DuckDNS has no official Terraform provider. This module creates no cloud
# resource: it renders a cloud-config part that makes the server update its own
# record at boot and every few minutes. An empty ip= lets DuckDNS use the
# address the request comes from, which is the server's public IP.

locals {
  fqdn       = "${var.subdomain}.duckdns.org"
  update_url = "https://www.duckdns.org/update?domains=${var.subdomain}&token=${var.token}&ip="

  cloud_init = templatefile("${path.module}/duckdns.cloud-init.yaml.tftpl", {
    update_url       = local.update_url
    interval_minutes = var.interval_minutes
  })
}
