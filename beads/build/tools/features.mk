# Copyright 2026 Insolent Construct.
# Author: Insolent Construct

# Rewrites the feature tables from beads/resources/features.json.
.PHONY: features test
features:
	python3 beads/build/tools/features.py

test:
	python3 beads/build/tools/features_test.py
