#!/usr/bin/env bash
# Name: Dummy Domain and Proxy Exporter
# Description: Exports small list domains for test
# Type: collector-domain 
# Stage: 1
# Provides: domain
# Install:
# InstallScope: shared


echo '{"bmop": "1.0", "module": "domain-collector", "pid": 11081}
{"t": "batch", "f": "domain", "c": 11}
--your-own-1password-account--.1password.com
033634aa-bd31-4486-abab-8a66a93bbf2c.prod.reverse.svc.fleetboard.com
03mn1500.com
04fabe21-8c9d-46a6-80d6-aaab87ad79df.prod.reverse.svc.fleetboard.com
1.gallileo-frankfurt.com
100-pine.com
100congressbuilding.com
dummy.termux.org
zabbix.pclender.org
zabka.pl
google.com
zabkagroup.com
{"t":"batch_end"}
{"t":"result","ok":true,"count":11}'


