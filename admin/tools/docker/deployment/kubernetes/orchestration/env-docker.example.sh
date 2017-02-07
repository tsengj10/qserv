# Configuration file sourced by Swarm scripts
# Allow to customize Docker container execution

# VERSION can be a git ticket branch but with _ instead of /
# example: tickets_DM-7139, or dev
VERSION=dev


# `docker run` settings
# =====================

# Customized configuration templates directory location
# on docker host, optional
# See <qserv-src-dir>/doc/source/HOW-TO/docker-custom-configuration.rst
# for additional information
HOST_CUSTOM_DIR=/qserv/custom

# Data directory location on docker host, optional
# HOST_DATA_DIR=/qserv/data

# Log directory location on docker host, optional
HOST_LOG_DIR=/qserv/log

# Qserv temporary directory location on docker host, optional
HOST_TMP_DIR=/qserv/tmp

# Use for debugging purpose
# Alternate command to execute at container startup
# in order no to launch Qserv at container startup
#ALT_CMD="tail -f /dev/null"


# Set images names
printf -v MASTER_IMAGE "qserv/qserv:%s_master" "$VERSION"
printf -v WORKER_IMAGE "qserv/qserv:%s_worker" "$VERSION"