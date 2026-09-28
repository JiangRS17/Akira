// Disable direct uk_lib_initcall for fabric-isolated library
@disable_liblwip_init@
@@
-uk_lib_initcall(liblwip_init);
+/* fabric-isolated: liblwip_init invoked via fabric gate */
