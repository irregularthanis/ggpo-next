# Unit tests for the DuckDNS module. It has no provider and creates no
# resources; these tests check what it renders.

variables {
  subdomain = "vw-friends"
  token     = "01234567-89ab-cdef-0123-456789abcdef"
}

run "renders_name_and_self_updating_record" {
  command = apply

  assert {
    condition     = output.fqdn == "vw-friends.duckdns.org"
    error_message = "fqdn must be the subdomain under duckdns.org."
  }
  assert {
    condition     = strcontains(output.cloud_init_part.content, "https://www.duckdns.org/update?domains=vw-friends&token=01234567-89ab-cdef-0123-456789abcdef&ip=")
    error_message = "The update URL must name the subdomain and leave ip= empty so DuckDNS uses the server's own address."
  }
  assert {
    condition     = strcontains(output.cloud_init_part.content, "OnUnitActiveSec=5min") && strcontains(output.cloud_init_part.content, "OnBootSec=30s")
    error_message = "The record must refresh at boot and every 5 minutes."
  }
  assert {
    condition     = strcontains(output.cloud_init_part.content, "permissions: \"0600\"")
    error_message = "The file holding the token must be readable by root only."
  }
  assert {
    condition     = strcontains(output.cloud_init_part.content, "ExecStart=/usr/bin/curl -fsS --max-time 20 -o /dev/null $${DUCKDNS_URL}")
    error_message = "systemd must expand the URL from its environment file, not from the unit."
  }
}

run "rejects_invalid_subdomain" {
  command = plan
  variables {
    subdomain = "Not_Valid"
  }
  expect_failures = [var.subdomain]
}

run "rejects_invalid_token" {
  command = plan
  variables {
    token = "not-a-token"
  }
  expect_failures = [var.token]
}
