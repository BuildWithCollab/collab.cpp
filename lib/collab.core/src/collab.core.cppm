export module collab.core;

export import :semver;
export import :manifest;
export import :log;
export import :term;
export import :signal;

export namespace collab::core {
    // Not constexpr: semver carries std::string fields (pre_release, build),
    // which make the type non-literal under libstdc++. MSVC accepts the
    // constexpr form, GCC rejects it. `inline const` is enough for the
    // single-definition guarantee across consumers.
    inline const semver version{1, 0, 0};
}
