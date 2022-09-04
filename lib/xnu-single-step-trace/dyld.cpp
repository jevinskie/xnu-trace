#include "common.h"

std::vector<image_info> get_dyld_image_infos(task_t target_task) {
    std::vector<image_info> res;
    task_dyld_info_data_t dyld_info;
    mach_msg_type_number_t cnt = TASK_DYLD_INFO_COUNT;
    mach_check(task_info(target_task, TASK_DYLD_INFO, (task_info_t)&dyld_info, &cnt),
               "task_info dyld info");
    assert(dyld_info.all_image_info_format == TASK_DYLD_ALL_IMAGE_INFO_64);
    const auto all_info_buf =
        read_target(target_task, dyld_info.all_image_info_addr, dyld_info.all_image_info_size);
    const dyld_all_image_infos *all_img_infos = (dyld_all_image_infos *)all_info_buf.data();

    if (!all_img_infos->infoArray) {
        return res;
    }

    res.emplace_back(image_info{.base = (uint64_t)all_img_infos->dyldImageLoadAddress,
                                .size = get_text_size(read_macho_segs_target(
                                    target_task, (uint64_t)all_img_infos->dyldImageLoadAddress)),
                                .path = read_cstr_target(target_task, all_img_infos->dyldPath),
                                .uuid = {}});

    const auto infos_buf = read_target(target_task, all_img_infos->infoArray,
                                       all_img_infos->infoArrayCount * sizeof(dyld_image_info));
    const auto img_infos = std::span<const dyld_image_info>{(dyld_image_info *)infos_buf.data(),
                                                            all_img_infos->infoArrayCount};
    for (const auto &img_info : img_infos) {
        const auto path = res.emplace_back(
            image_info{.base = (uint64_t)img_info.imageLoadAddress,
                       .size = get_text_size(read_macho_segs_target(
                           target_task, (uint64_t)img_info.imageLoadAddress)),
                       .path = read_cstr_target(target_task, img_info.imageFilePath),
                       .uuid = {}});
    }

    std::sort(res.begin(), res.end());

#if 0
    for (const auto &img_info : res) {
        fmt::print("img_info base: {:#018x} sz: {:#010x} path: '{:s}'\n", img_info.base,
                   img_info.size, img_info.path.string());
    }
#endif

    return res;
}
