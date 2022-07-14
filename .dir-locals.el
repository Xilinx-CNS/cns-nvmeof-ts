((nil
  (eval . (let ((root (projectile-project-root)))
            (setq-local company-clang-arguments
                        (list (concat "-I" root "nvmeof-ts/lib")
                              (concat "-I" root "build/inst/default/include")
                              )
                        )
            (setq-local flycheck-clang-include-path
                        (list (concat root "nvmeof-ts/lib")
                              (concat root "build/inst/default/include")
                              )
                        )
            (setq-local flycheck-clang-args
                        (list
                              (concat "-Wno-error=unused-local-typedefs ")
                              (concat "-Wno-unused-local-typedefs")
                              (concat "-Wno-error=unused-parameters ")
                              (concat "-Wno-unused-parameters")
                              (concat "-Wno-error=unused-function ")
                              (concat "-Wno-error=unused-function ")
                              (concat "-Wno-error=unused-struct-member")
                              (concat "-Wno-unused-struct-member")
                              )
                        )
))))
