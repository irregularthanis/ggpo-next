# Vector War matchmaking server, brought up and destroyed by hand around play
# sessions. CI only tests this configuration with mocked providers.

module "duckdns" {
  source    = "../../modules/duckdns"
  subdomain = var.duckdns_subdomain
  token     = var.duckdns_token
}

module "host" {
  source           = "../../modules/host"
  compartment_ocid = var.compartment_ocid
  container_image  = var.container_image
  server_port      = var.server_port
  admin_cidr       = var.admin_cidr
  ssh_public_key   = var.ssh_public_key
  extra_cloud_init = [module.duckdns.cloud_init_part]
}
