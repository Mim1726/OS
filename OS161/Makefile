MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
COMPOSE_FILE := $(MAKEFILE_DIR)docker-compose.yml
COMPOSE ?= docker compose -f $(COMPOSE_FILE)
SERVICE ?= os161

.PHONY: up down shell

up:
	$(COMPOSE) up -d

down:
	$(COMPOSE) down

shell:
	$(COMPOSE) exec $(SERVICE) /bin/bash