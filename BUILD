cc_library(
    name = "qpc",
    srcs = [
        "source/qep_hsm.c",
        "source/qep_msm.c",
        "source/qf_act.c",
        "source/qf_actq.c",
        "source/qf_defer.c",
        "source/qf_dyn.c",
        "source/qf_mem.c",
        "source/qf_ps.c",
        "source/qf_qact.c",
        "source/qf_qeq.c",
        "source/qf_qmact.c",
        "source/qf_time.c",
        "source/qf_pkg.h",
        "ports/posix/qf_port.c",
        "source/qs.c",
        "source/qs_rx.c",
        "source/qs_fp.c",
        "source/qs_64bit.c",
        "source/qs_pkg.h",
        "source/qk_mutex.c",
        "source/qk.c",
        "source/qk_pkg.h",
    ],
    hdrs = [
        "include/qassert.h",
        "include/qep.h",
        "ports/posix/qep_port.h",
        "include/qequeue.h",
        "include/qf.h",
        "ports/posix/qf_port.h",
        "include/qk.h",
        "include/qmpool.h",
        "include/qp_port.h",
        "include/qpc.h",
        "include/qpset.h",
        "include/qs_dummy.h",
        "include/qs.h",
        "ports/posix/qs_port.h",
    ],
    copts = [
        "-Wall",
        "-DQ_SPY",
    ],
    linkstatic = 1,
    visibility = ["//visibility:public"],
)