/*  Copyright (c) 2010-2015 L-SMASH project */

/*  Must update this file when updating to a new version of l-smash */

typedef struct
{
  uint8_t present;
  uint8_t CpbDpbDelaysPresentFlag;
  uint8_t cpb_removal_delay_length;
  uint8_t dpb_output_delay_length;
} h264_hrd_t;

typedef struct
{
  uint16_t sar_width;
  uint16_t sar_height;
  uint8_t  video_full_range_flag;
  uint8_t  colour_primaries;
  uint8_t  transfer_characteristics;
  uint8_t  matrix_coefficients;
  uint32_t num_units_in_tick;
  uint32_t time_scale;
  uint8_t  fixed_frame_rate_flag;
  uint8_t  pic_struct_present_flag;
  h264_hrd_t hrd;
} h264_vui_t;

typedef struct
{
  uint8_t  present;
  uint8_t  profile_idc;
  uint8_t  constraint_set_flags;
  uint8_t  level_idc;
  uint8_t  seq_parameter_set_id;
  uint8_t  chroma_format_idc;
  uint8_t  separate_colour_plane_flag;
  uint8_t  ChromaArrayType;
  uint8_t  bit_depth_luma_minus8;
  uint8_t  bit_depth_chroma_minus8;
  uint8_t  log2_max_frame_num;
  uint8_t  pic_order_cnt_type;
  uint8_t  delta_pic_order_always_zero_flag;
  uint8_t  num_ref_frames_in_pic_order_cnt_cycle;
  uint8_t  frame_mbs_only_flag;
  int32_t  offset_for_non_ref_pic;
  int32_t  offset_for_top_to_bottom_field;
  int32_t  offset_for_ref_frame[255];
  int64_t  ExpectedDeltaPerPicOrderCntCycle;
  uint32_t max_num_ref_frames;
  uint32_t MaxFrameNum;
  uint32_t log2_max_pic_order_cnt_lsb;
  uint32_t MaxPicOrderCntLsb;
  uint32_t PicSizeInMapUnits;
  uint32_t cropped_width;
  uint32_t cropped_height;
  h264_vui_t vui;
} h264_sps_t;

typedef struct
{
  uint8_t  present;
  uint8_t  pic_parameter_set_id;
  uint8_t  seq_parameter_set_id;
  uint8_t  entropy_coding_mode_flag;
  uint8_t  bottom_field_pic_order_in_frame_present_flag;
  uint8_t  num_slice_groups_minus1;
  uint8_t  slice_group_map_type;
  uint8_t  num_ref_idx_l0_default_active_minus1;
  uint8_t  num_ref_idx_l1_default_active_minus1;
  uint8_t  weighted_pred_flag;
  uint8_t  weighted_bipred_idc;
  uint8_t  deblocking_filter_control_present_flag;
  uint8_t  redundant_pic_cnt_present_flag;
  uint32_t SliceGroupChangeRate;
} h264_pps_t;

typedef struct
{
  uint8_t present;
  uint8_t pic_struct;
} h264_pic_timing_t;

typedef struct
{
  uint8_t  present;
  uint8_t  random_accessible;
  uint8_t  broken_link_flag;
  uint32_t recovery_frame_cnt;
} h264_recovery_point_t;

typedef struct
{
  h264_pic_timing_t     pic_timing;
  h264_recovery_point_t recovery_point;
} h264_sei_t;

typedef struct
{
  uint8_t  present;
  uint8_t  slice_id;  /* only for slice data partition */
  uint8_t  type;
  uint8_t  pic_order_cnt_type;
  uint8_t  nal_ref_idc;
  uint8_t  IdrPicFlag;
  uint8_t  seq_parameter_set_id;
  uint8_t  pic_parameter_set_id;
  uint8_t  field_pic_flag;
  uint8_t  bottom_field_flag;
  uint8_t  has_mmco5;
  uint8_t  has_redundancy;
  uint16_t idr_pic_id;
  uint32_t frame_num;
  int32_t  pic_order_cnt_lsb;
  int32_t  delta_pic_order_cnt_bottom;
  int32_t  delta_pic_order_cnt[2];
} h264_slice_info_t;

typedef enum
{
  H264_PICTURE_TYPE_IDR         = 0,
  H264_PICTURE_TYPE_I           = 1,
  H264_PICTURE_TYPE_I_P         = 2,
  H264_PICTURE_TYPE_I_P_B       = 3,
  H264_PICTURE_TYPE_SI          = 4,
  H264_PICTURE_TYPE_SI_SP       = 5,
  H264_PICTURE_TYPE_I_SI        = 6,
  H264_PICTURE_TYPE_I_SI_P_SP   = 7,
  H264_PICTURE_TYPE_I_SI_P_SP_B = 8,
  H264_PICTURE_TYPE_NONE        = 9,
} h264_picture_type;

typedef struct
{
  h264_picture_type type;
  uint8_t           idr;
  uint8_t           random_accessible;
  uint8_t           independent;
  uint8_t           disposable;   /* 1: nal_ref_idc == 0, 0: otherwise */
  uint8_t           has_redundancy;
  uint8_t           has_primary;
  uint8_t           pic_parameter_set_id;
  uint8_t           field_pic_flag;
  uint8_t           bottom_field_flag;
  uint8_t           delta;
  uint8_t           broken_link_flag;
  /* POC */
  uint8_t           has_mmco5;
  uint8_t           ref_pic_has_mmco5;
  uint8_t           ref_pic_bottom_field_flag;
  int32_t           ref_pic_TopFieldOrderCnt;
  int32_t           ref_pic_PicOrderCntMsb;
  int32_t           ref_pic_PicOrderCntLsb;
  int32_t           pic_order_cnt_lsb;
  int32_t           delta_pic_order_cnt_bottom;
  int32_t           delta_pic_order_cnt[2];
  int32_t           PicOrderCnt;
  uint32_t          FrameNumOffset;
  /* */
  uint32_t          recovery_frame_cnt;
  uint32_t          frame_num;
} h264_picture_info_t;

typedef struct
{
  uint8_t            *data;
  uint8_t            *incomplete_data;
  uint32_t            length;
  uint32_t            incomplete_length;
  uint32_t            number;
  h264_picture_info_t picture;
} h264_access_unit_t;

typedef struct
{
  int      unseekable;    /* If set to 1, the buffer is unseekable. */
  int      internal;      /* If set to 1, the buffer is allocated on heap internally.
                           * The pointer to the buffer shall not be changed by any method other than internal allocation. */
  uint8_t *data;          /* the pointer to the buffer for reading/writing */
  size_t   store;         /* valid data size on the buffer */
  size_t   alloc;         /* total buffer size including invalid area */
  size_t   pos;           /* the data position on the buffer to be read next */
  size_t   max_size;      /* the maximum number of bytes for reading from the stream at one time */
  uint64_t count;         /* counter for arbitrary usage */
} lsmash_buffer_t;

typedef struct
{
  void           *stream;         /* I/O stream */
  uint8_t         eof;            /* If set to 1, the stream reached EOF. */
  uint8_t         eob;            /* if set to 1, we cannot read more bytes from the stream and the buffer until any seek. */
  uint8_t         error;          /* If set to 1, any error is detected. */
  uint8_t         unseekable;     /* If set to 1, the stream is unseekable. */
  uint64_t        written;        /* the number of bytes written into 'stream' already */
  uint64_t        offset;         /* the current position in the 'stream'
                                   * the number of bytes from the beginning */
  lsmash_buffer_t buffer;
  int     (*read) ( void *opaque, uint8_t *buf, int size );
  int     (*write)( void *opaque, uint8_t *buf, int size );
  int64_t (*seek) ( void *opaque, int64_t offset, int whence );
} lsmash_bs_t;

typedef struct {
  lsmash_bs_t* bs;
  uint8_t store;
  uint8_t cache;
} lsmash_bits_t;


typedef struct lsmash_entry_tag lsmash_entry_t;

struct lsmash_entry_tag
{
  lsmash_entry_t *next;
  lsmash_entry_t *prev;
  void *data;
};

typedef struct
{
  lsmash_entry_t *head;
  lsmash_entry_t *tail;
  lsmash_entry_t *last_accessed_entry;
  uint32_t last_accessed_number;
  uint32_t entry_count;
} lsmash_entry_list_t;

typedef struct
{
  uint32_t number_of_buffers;
  uint32_t buffer_size;
  void    *buffers;
} lsmash_multiple_buffers_t;

typedef struct
{
  lsmash_multiple_buffers_t *bank;
  uint8_t                   *rbsp;
} h264_stream_buffer_t;

typedef struct h264_info_tag h264_info_t;

struct h264_info_tag
{
  lsmash_h264_specific_parameters_t avcC_param;
  lsmash_h264_specific_parameters_t avcC_param_next;
  lsmash_entry_list_t  sps_list  [1]; /* contains entries as h264_sps_t */
  lsmash_entry_list_t  pps_list  [1]; /* contains entries as h264_pps_t */
  lsmash_entry_list_t  slice_list[1]; /* for slice data partition */
  h264_sps_t           sps;           /* active SPS */
  h264_pps_t           pps;           /* active PPS */
  h264_sei_t           sei;           /* active SEI */
  h264_slice_info_t    slice;         /* active slice */
  h264_access_unit_t   au;
  uint8_t              prev_nalu_type;
  uint8_t              avcC_pending;
  lsmash_bits_t       *bits;
  h264_stream_buffer_t buffer;
};

extern int h264_setup_parser(h264_info_t* info, int parse_only);
extern int h264_parse_sps(h264_info_t* info, uint8_t* rbsp_buffer, uint8_t* ebsp, uint64_t ebsp_size);
extern void h264_cleanup_parser(h264_info_t *info);