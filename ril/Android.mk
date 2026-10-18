# all-makefiles-under del Android.mk raiz solo baja un nivel: sin este archivo
# libril y rilshim no entran al build completo (solo con mmm).
include $(call all-subdir-makefiles)
