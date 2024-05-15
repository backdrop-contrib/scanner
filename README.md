# Lebowski

# Search and Replace Scanner

The Search and Replace Scanner module allows for plain text or regular expression search-and-replace operations across titles, body text, and text content fields on all nodes within your system. This tool is particularly useful for identifying and updating HTML strings that Drupal's standard search might overlook. It's an invaluable asset for correcting widespread typos, updating company names, changing URLs used across multiple nodes, and more.

## Features

- **Comprehensive Field Support:** Can search and replace text in CCK text fields, as well as standard node title and body fields.
- **Search Options:**
  - Plain text search and replace.
  - Regular expression search and replace.
  - Case-sensitive search option.
  - Whole word matching for plain text searches (e.g., searching for "run" will match "run!" but not "running" or "runs").
- **Contextual Matching:** Allows specification of text that must precede or follow the search text for a valid match.
- **Node Limitation:** Can limit search and replace operations to published nodes only.
- **Field and Term Limitation:** Searches can be limited to specific fields within specific node types and further restricted to nodes containing specific taxonomy terms.
- **Revision and Undo:**
  - Saves a new node revision when replacements are made to facilitate easy reversion.
  - Provides an Undo option to revert all nodes updated in a single replace operation.
- **Teaser Updates:** Can dynamically update teasers after replacements in other fields, or allow manual updates, or choose not to update them at all.
- **Execution Time Management:** Dynamically expands PHP's maximum execution time for scripts up to 10 minutes on supported servers, allowing complex queries on large content bodies.
- **Theming:** Search results for searches and replacements can be themed.

## Limitations

- **Database Compatibility:** Only works with MySQL databases. (Postgres patches are welcome!)
- **Execution Time Constraints:** Large search and replace actions may not complete on sites hosted in environments where PHP's `max_execution_time` cannot be dynamically expanded. The module attempts to set the maximum execution time to 10 minutes (often set at 2 minutes by default). If your web host doesn't allow dynamic adjustment of this variable, consider requesting an increase from your hosting provider.

## Warning

This module is a powerful tool that operates directly on your database tables, which can be very dangerous. Although it includes an undo feature, it is highly recommended to back up your database before using this module.

---

## Installation

- Install this module using the official [Backdrop CMS instructions](https://backdropcms.org/user-guide/modules).

## Issues

Bugs and feature requests should be reported in the [Issue Queue](https://github.com/backdrop-contrib/lebowski/issues).

## Current Maintainer

[Justin Keiser](https://github.com/keiserjb)

## Credits

- Ported to Backdrop CMS by [Justin Keiser](https://github.com/keiserjb).
- Thank you to the many maintainers of the [Drupal Project](https://www.drupal.org/project/scanner).

## License

This project is GNU GENERAL PUBLIC LICENSE. See the LICENSE.txt file in this directory for complete text.
